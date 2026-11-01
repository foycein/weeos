#include "Media/JpegScanDecoder.h"

#include "Formats/FormatError.h"

namespace Media
{

namespace
{

constexpr int LastCoefficient = CoefficientsPerBlock - 1;

// An AC value of the Huffman table holds two numbers: the count of zero coefficients before the next coefficient (high four
// bits) and how many bits that coefficient has (low four bits).
constexpr int RunShift = 4;
constexpr int SizeMask = 0x0F;

// A size of 0 with a run of 15 means sixteen zero coefficients (ZRL); with a smaller run it ends the block or band.
constexpr int SixteenZerosRun = 15;
constexpr int ZerosInSixteenZerosRun = 16;

}

JpegScanDecoder::JpegScanDecoder(JpegFrame& frameToFill, const JpegScan& scanToDecode, const JpegHuffmanTables& definedTables, int interval)
    : frame(frameToFill)
    , scan(scanToDecode)
    , tables(definedTables)
    , restartInterval(interval)
{
}

void JpegScanDecoder::Decode(JpegBitReader& bits)
{
    for (int componentIndex : scan.componentIndexes)
        frame.components[componentIndex].dcPrediction = 0;

    if (scan.componentIndexes.size() == 1)
        DecodeOneComponent(bits);
    else
        DecodeInterleaved(bits);
}

// A scan of one component sends its blocks row by row, only the blocks that hold samples, and every block is a unit of its own.
void JpegScanDecoder::DecodeOneComponent(JpegBitReader& bits)
{
    JpegComponent& component = frame.components[scan.componentIndexes[0]];
    int unitsUntilRestart = restartInterval;

    for (int blockRow = 0; blockRow < component.usedBlocksPerColumn; ++blockRow)
    {
        for (int blockColumn = 0; blockColumn < component.usedBlocksPerLine; ++blockColumn)
        {
            RestartIfDue(bits, unitsUntilRestart);
            DecodeBlock(component, component.BlockAt(blockRow, blockColumn), bits);
        }
    }
}

// An interleaved scan sends MCU after MCU; in each, the blocks of each component in turn, row by row inside the MCU.
void JpegScanDecoder::DecodeInterleaved(JpegBitReader& bits)
{
    int unitsUntilRestart = restartInterval;

    for (int mcuRow = 0; mcuRow < frame.mcusPerColumn; ++mcuRow)
    {
        for (int mcuColumn = 0; mcuColumn < frame.mcusPerLine; ++mcuColumn)
        {
            RestartIfDue(bits, unitsUntilRestart);

            for (int componentIndex : scan.componentIndexes)
            {
                JpegComponent& component = frame.components[componentIndex];
                for (int rowInMcu = 0; rowInMcu < component.verticalSampling; ++rowInMcu)
                {
                    for (int columnInMcu = 0; columnInMcu < component.horizontalSampling; ++columnInMcu)
                    {
                        int blockRow = mcuRow * component.verticalSampling + rowInMcu;
                        int blockColumn = mcuColumn * component.horizontalSampling + columnInMcu;
                        DecodeBlock(component, component.BlockAt(blockRow, blockColumn), bits);
                    }
                }
            }
        }
    }
}

void JpegScanDecoder::RestartIfDue(JpegBitReader& bits, int& unitsUntilRestart)
{
    if (restartInterval == 0)
        return;

    if (unitsUntilRestart == 0)
    {
        bits.Restart();
        for (int componentIndex : scan.componentIndexes)
            frame.components[componentIndex].dcPrediction = 0;

        endOfBandRun = 0;
        unitsUntilRestart = restartInterval;
    }

    --unitsUntilRestart;
}

void JpegScanDecoder::DecodeBlock(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits)
{
    if (!frame.isProgressive)
    {
        DecodeWholeBlock(component, block, bits);
        return;
    }

    bool isFirstScanOfTheseBits = scan.successiveHigh == 0;
    bool isAverageScan = scan.spectralStart == 0;

    if (isAverageScan && isFirstScanOfTheseBits)
        DecodeFirstAverage(component, block, bits);
    else if (isAverageScan)
        RefineAverage(block, bits);
    else if (isFirstScanOfTheseBits)
        DecodeFirstBand(component, block, bits);
    else
        RefineBand(component, block, bits);
}

void JpegScanDecoder::DecodeWholeBlock(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits)
{
    block[0] = static_cast<std::int16_t>(ReadAverageDifference(component, bits));

    const JpegHuffmanTable& acTable = AcTableOf(component);
    int zigZagPosition = 1;
    while (zigZagPosition <= LastCoefficient)
    {
        int value = acTable.ReadValue(bits);
        int zeroRun = value >> RunShift;
        int size = value & SizeMask;

        if (size == 0)
        {
            if (zeroRun != SixteenZerosRun)
                return;

            zigZagPosition += ZerosInSixteenZerosRun;
            continue;
        }

        zigZagPosition += zeroRun;
        if (zigZagPosition > LastCoefficient)
            return;

        block[ZigZagToRowByRow[zigZagPosition]] = static_cast<std::int16_t>(bits.ReadSignedNumber(size));
        ++zigZagPosition;
    }
}

void JpegScanDecoder::DecodeFirstAverage(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits)
{
    int average = ReadAverageDifference(component, bits);
    block[0] = static_cast<std::int16_t>(average * (1 << scan.successiveLow));
}

void JpegScanDecoder::RefineAverage(CoefficientBlock& block, JpegBitReader& bits)
{
    if (bits.ReadBit() == 1)
        block[0] = static_cast<std::int16_t>(block[0] | (1 << scan.successiveLow));
}

void JpegScanDecoder::DecodeFirstBand(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits)
{
    if (endOfBandRun > 0)
    {
        --endOfBandRun;
        return;
    }

    const JpegHuffmanTable& acTable = AcTableOf(component);
    int zigZagPosition = scan.spectralStart;
    while (zigZagPosition <= scan.spectralEnd)
    {
        int value = acTable.ReadValue(bits);
        int zeroRun = value >> RunShift;
        int size = value & SizeMask;

        if (size == 0)
        {
            if (zeroRun == SixteenZerosRun)
            {
                zigZagPosition += ZerosInSixteenZerosRun;
                continue;
            }

            // This block ends the band and so do the next (2^run - 1 + the bits that follow) blocks.
            endOfBandRun = (1 << zeroRun) - 1 + bits.ReadBits(zeroRun);
            return;
        }

        zigZagPosition += zeroRun;
        if (zigZagPosition > LastCoefficient)
            return;

        int coefficient = bits.ReadSignedNumber(size) * (1 << scan.successiveLow);
        block[ZigZagToRowByRow[zigZagPosition]] = static_cast<std::int16_t>(coefficient);
        ++zigZagPosition;
    }
}

// A refining scan sends one more bit of every coefficient of the band (G.1.2.3). A coefficient that is not zero yet gets its bit
// as a correction bit. A coefficient that becomes non-zero in this scan is coded like in a first scan, with a size of 1, and the run
// before it counts only the coefficients that are still zero.
void JpegScanDecoder::RefineBand(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits)
{
    int zigZagPosition = scan.spectralStart;
    if (endOfBandRun == 0)
        zigZagPosition = RefineCodedCoefficients(component, block, bits);

    if (endOfBandRun == 0)
        return;

    for (; zigZagPosition <= scan.spectralEnd; ++zigZagPosition)
    {
        std::int16_t& coefficient = block[ZigZagToRowByRow[zigZagPosition]];
        if (coefficient != 0)
            RefineNonZeroCoefficient(coefficient, bits);
    }
    --endOfBandRun;
}

// Reads the codes of the band up to its end or up to an end-of-band run, and gives the place where the codes stopped.
int JpegScanDecoder::RefineCodedCoefficients(JpegComponent& component, CoefficientBlock& block, JpegBitReader& bits)
{
    const JpegHuffmanTable& acTable = AcTableOf(component);
    int newCoefficient = 1 << scan.successiveLow;
    int zigZagPosition = scan.spectralStart;

    while (zigZagPosition <= scan.spectralEnd)
    {
        int value = acTable.ReadValue(bits);
        int zerosToSkip = value >> RunShift;
        int size = value & SizeMask;

        bool startsEndOfBandRun = size == 0 && zerosToSkip != SixteenZerosRun;
        if (startsEndOfBandRun)
        {
            endOfBandRun = (1 << zerosToSkip) + bits.ReadBits(zerosToSkip);
            return zigZagPosition;
        }

        int valueToPlace = 0;
        if (size != 0)
            valueToPlace = bits.ReadBit() == 1 ? newCoefficient : -newCoefficient;

        zigZagPosition = SkipZeroCoefficients(block, zigZagPosition, zerosToSkip, bits);
        if (valueToPlace != 0 && zigZagPosition <= scan.spectralEnd)
            block[ZigZagToRowByRow[zigZagPosition]] = static_cast<std::int16_t>(valueToPlace);

        ++zigZagPosition;
    }
    return zigZagPosition;
}

// Steps over zerosToSkip coefficients that are still zero and stops on the next zero one. The coefficients that are not zero
// on the way get their correction bit.
int JpegScanDecoder::SkipZeroCoefficients(CoefficientBlock& block, int zigZagPosition, int zerosToSkip, JpegBitReader& bits)
{
    while (zigZagPosition <= scan.spectralEnd)
    {
        std::int16_t& coefficient = block[ZigZagToRowByRow[zigZagPosition]];
        if (coefficient != 0)
            RefineNonZeroCoefficient(coefficient, bits);
        else if (zerosToSkip == 0)
            return zigZagPosition;
        else
            --zerosToSkip;

        ++zigZagPosition;
    }
    return zigZagPosition;
}

void JpegScanDecoder::RefineNonZeroCoefficient(std::int16_t& coefficient, JpegBitReader& bits)
{
    int bit = 1 << scan.successiveLow;
    if (bits.ReadBit() == 0)
        return;

    if ((coefficient & bit) != 0)
        return;

    if (coefficient > 0)
        coefficient = static_cast<std::int16_t>(coefficient + bit);
    else
        coefficient = static_cast<std::int16_t>(coefficient - bit);
}

int JpegScanDecoder::ReadAverageDifference(JpegComponent& component, JpegBitReader& bits)
{
    int size = DcTableOf(component).ReadValue(bits);
    component.dcPrediction += bits.ReadSignedNumber(size);
    return component.dcPrediction;
}

const JpegHuffmanTable& JpegScanDecoder::DcTableOf(const JpegComponent& component) const
{
    const JpegHuffmanTable& table = tables.dc[component.dcTableIndex];
    if (!table.IsDefined())
        throw Formats::FormatError("JPEG picture: a scan uses a DC Huffman table that is not defined");

    return table;
}

const JpegHuffmanTable& JpegScanDecoder::AcTableOf(const JpegComponent& component) const
{
    const JpegHuffmanTable& table = tables.ac[component.acTableIndex];
    if (!table.IsDefined())
        throw Formats::FormatError("JPEG picture: a scan uses an AC Huffman table that is not defined");

    return table;
}

}
