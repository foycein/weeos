#include "Media/JpegDecoder.h"

#include "Media/JpegBitReader.h"
#include "Media/JpegFrame.h"
#include "Media/JpegHuffmanTable.h"
#include "Media/JpegPicture.h"
#include "Media/JpegScanDecoder.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <format>
#include <string_view>

namespace Media
{

namespace
{

constexpr std::string_view DataDescription = "JPEG picture";

constexpr Formats::Byte MarkerPrefix = 0xFF;
constexpr Formats::Byte StartOfImage = 0xD8;
constexpr Formats::Byte EndOfImage = 0xD9;
constexpr Formats::Byte BaselineFrame = 0xC0;
constexpr Formats::Byte ExtendedFrame = 0xC1;
constexpr Formats::Byte ProgressiveFrame = 0xC2;
constexpr Formats::Byte DefineHuffmanTables = 0xC4;
constexpr Formats::Byte DefineArithmeticConditioning = 0xCC;
constexpr Formats::Byte FirstFrameMarker = 0xC0;
constexpr Formats::Byte LastFrameMarker = 0xCF;
constexpr Formats::Byte FirstRestartMarker = 0xD0;
constexpr Formats::Byte LastRestartMarker = 0xD7;
constexpr Formats::Byte StartOfScan = 0xDA;
constexpr Formats::Byte DefineQuantizationTables = 0xDB;
constexpr Formats::Byte DefineRestartInterval = 0xDD;
constexpr Formats::Byte AdobeSegment = 0xEE;
constexpr Formats::Byte StuffedZero = 0x00;

constexpr int SamplePrecision = 8;
constexpr int GrayComponentCount = 1;
constexpr int ColorComponentCount = 3;
constexpr int LargestSamplingFactor = 4;
constexpr int SegmentLengthSize = 2;

constexpr int DcTableClass = 0;
constexpr int AcTableClass = 1;
constexpr int SixteenBitQuantizationEntries = 1;
constexpr Formats::Byte AdobeTransformNone = 0;
constexpr std::string_view AdobeSignature = "Adobe";
constexpr std::size_t AdobeSegmentSize = 12;
constexpr std::size_t AdobeVersionAndFlagsSize = 6;

int HighNibble(Formats::Byte value)
{
    return value >> 4;
}

int LowNibble(Formats::Byte value)
{
    return value & 0x0F;
}

int DivideRoundingUp(int value, int divisor)
{
    return (value + divisor - 1) / divisor;
}

bool IsRestartMarker(Formats::Byte marker)
{
    return marker >= FirstRestartMarker && marker <= LastRestartMarker;
}

// The marker walk of a JPEG file (Annex B): segments with a length, scans with their entropy coded data behind them.
class JpegFileReader
{
public:
    explicit JpegFileReader(Formats::ByteView bytes)
        : fileBytes(bytes)
        , reader(bytes, DataDescription)
    {
        frameHuffmanTables.dc[0] = JpegHuffmanTable(StandardLuminanceDcTable());
        frameHuffmanTables.dc[1] = JpegHuffmanTable(StandardChrominanceDcTable());
        frameHuffmanTables.ac[0] = JpegHuffmanTable(StandardLuminanceAcTable());
        frameHuffmanTables.ac[1] = JpegHuffmanTable(StandardChrominanceAcTable());
    }

    JpegInformation ReadUpToFrame()
    {
        ReadStartOfImage();
        while (!hasFrame)
            ReadNextSegment();

        return {frame.width, frame.height, frame.isProgressive};
    }

    Formats::RgbaImage Decode()
    {
        ReadStartOfImage();
        while (!hasReachedEnd && reader.RemainingSize() >= SegmentLengthSize)
            ReadNextSegment();

        if (!hasFrame)
            reader.Fail("the picture has no frame header");

        return MakeJpegPicture(frame, quantizationTables, colors);
    }

private:
    void ReadStartOfImage()
    {
        if (reader.ReadByte() != MarkerPrefix || reader.ReadByte() != StartOfImage)
            reader.Fail("a JPEG picture starts with the marker SOI");
    }

    void ReadNextSegment()
    {
        Formats::Byte marker = ReadMarker();
        if (marker == EndOfImage)
        {
            hasReachedEnd = true;
            return;
        }

        if (IsRestartMarker(marker))
            return;

        if (marker == StartOfScan)
        {
            ReadScan();
            return;
        }

        std::uint16_t segmentLength = reader.ReadUint16();
        if (segmentLength < SegmentLengthSize)
            reader.Fail("a segment is shorter than its length field");

        Formats::ByteReader segment(reader.ReadBytes(segmentLength - SegmentLengthSize), DataDescription);
        ReadSegment(marker, segment);
    }

    // A marker is 0xFF and a code. Any number of 0xFF may come before it as fill; other bytes between segments are skipped.
    Formats::Byte ReadMarker()
    {
        while (reader.ReadByte() != MarkerPrefix)
        {
        }

        Formats::Byte marker = reader.ReadByte();
        while (marker == MarkerPrefix)
            marker = reader.ReadByte();

        return marker;
    }

    void ReadSegment(Formats::Byte marker, Formats::ByteReader& segment)
    {
        if (marker == BaselineFrame || marker == ExtendedFrame)
            ReadFrame(segment, false);
        else if (marker == ProgressiveFrame)
            ReadFrame(segment, true);
        else if (marker == DefineHuffmanTables)
            ReadHuffmanTables(segment);
        else if (marker == DefineQuantizationTables)
            ReadQuantizationTables(segment);
        else if (marker == DefineRestartInterval)
            restartInterval = segment.ReadUint16();
        else if (marker == AdobeSegment)
            ReadAdobeSegment(segment);
        else if (marker >= FirstFrameMarker && marker <= LastFrameMarker && marker != DefineArithmeticConditioning)
            segment.Fail(std::format("the frame kind {:#04x} (arithmetic coding, lossless or hierarchical) is not supported", marker));
    }

    void ReadFrame(Formats::ByteReader& segment, bool isProgressive)
    {
        if (hasFrame)
            segment.Fail("the picture has a second frame header");

        if (segment.ReadByte() != SamplePrecision)
            segment.Fail("only samples of 8 bits are supported");

        frame.isProgressive = isProgressive;
        frame.height = segment.ReadUint16();
        frame.width = segment.ReadUint16();
        if (frame.width == 0 || frame.height == 0)
            segment.Fail("the picture has no width or no height");

        int componentCount = segment.ReadByte();
        if (componentCount != GrayComponentCount && componentCount != ColorComponentCount)
            segment.Fail(std::format("pictures with {} components are not supported", componentCount));

        for (int componentNumber = 0; componentNumber < componentCount; ++componentNumber)
            frame.components.push_back(ReadFrameComponent(segment));

        PrepareBlocks();
        hasFrame = true;
    }

    JpegComponent ReadFrameComponent(Formats::ByteReader& segment)
    {
        JpegComponent component;
        component.id = segment.ReadByte();
        Formats::Byte sampling = segment.ReadByte();
        component.horizontalSampling = HighNibble(sampling);
        component.verticalSampling = LowNibble(sampling);
        component.quantizationTableIndex = segment.ReadByte();

        bool isSamplingValid = component.horizontalSampling >= 1 && component.horizontalSampling <= LargestSamplingFactor
            && component.verticalSampling >= 1 && component.verticalSampling <= LargestSamplingFactor;
        if (!isSamplingValid)
            segment.Fail("a component has a sampling factor outside 1 to 4");

        if (component.quantizationTableIndex >= QuantizationTableSlots)
            segment.Fail("a component names a quantization table that cannot exist");

        return component;
    }

    void PrepareBlocks()
    {
        for (const JpegComponent& component : frame.components)
        {
            frame.largestHorizontalSampling = std::max(frame.largestHorizontalSampling, component.horizontalSampling);
            frame.largestVerticalSampling = std::max(frame.largestVerticalSampling, component.verticalSampling);
        }

        frame.mcusPerLine = DivideRoundingUp(frame.width, BlockSide * frame.largestHorizontalSampling);
        frame.mcusPerColumn = DivideRoundingUp(frame.height, BlockSide * frame.largestVerticalSampling);

        for (JpegComponent& component : frame.components)
        {
            component.sampleWidth = DivideRoundingUp(frame.width * component.horizontalSampling, frame.largestHorizontalSampling);
            component.sampleHeight = DivideRoundingUp(frame.height * component.verticalSampling, frame.largestVerticalSampling);
            component.usedBlocksPerLine = DivideRoundingUp(component.sampleWidth, BlockSide);
            component.usedBlocksPerColumn = DivideRoundingUp(component.sampleHeight, BlockSide);
            component.blocksPerLine = frame.mcusPerLine * component.horizontalSampling;
            component.blocksPerColumn = frame.mcusPerColumn * component.verticalSampling;
            component.blocks.assign(static_cast<std::size_t>(component.blocksPerLine) * component.blocksPerColumn, CoefficientBlock{});
        }
    }

    void ReadHuffmanTables(Formats::ByteReader& segment)
    {
        while (!segment.IsAtEnd())
        {
            Formats::Byte classAndSlot = segment.ReadByte();
            int tableClass = HighNibble(classAndSlot);
            int slot = LowNibble(classAndSlot);
            if (slot >= HuffmanTableSlots || (tableClass != DcTableClass && tableClass != AcTableClass))
                segment.Fail("a Huffman table has a class or a slot that cannot exist");

            HuffmanTableDefinition definition;
            int valueCount = 0;
            for (int& count : definition.codeCountOfLength)
            {
                count = segment.ReadByte();
                valueCount += count;
            }

            Formats::ByteView values = segment.ReadBytes(valueCount);
            definition.values.assign(values.begin(), values.end());

            if (tableClass == DcTableClass)
                frameHuffmanTables.dc[slot] = JpegHuffmanTable(definition);
            else
                frameHuffmanTables.ac[slot] = JpegHuffmanTable(definition);
        }
    }

    void ReadQuantizationTables(Formats::ByteReader& segment)
    {
        while (!segment.IsAtEnd())
        {
            Formats::Byte precisionAndSlot = segment.ReadByte();
            int slot = LowNibble(precisionAndSlot);
            if (slot >= QuantizationTableSlots)
                segment.Fail("a quantization table has a slot that cannot exist");

            bool hasSixteenBitEntries = HighNibble(precisionAndSlot) == SixteenBitQuantizationEntries;
            QuantizationTable& table = quantizationTables[slot];
            for (int zigZagPosition = 0; zigZagPosition < CoefficientsPerBlock; ++zigZagPosition)
            {
                std::uint16_t entry = hasSixteenBitEntries ? segment.ReadUint16() : segment.ReadByte();
                table[ZigZagToRowByRow[zigZagPosition]] = entry;
            }
        }
    }

    // Adobe's segment says whether the three components are RGB as they are (transform 0) or YCbCr.
    void ReadAdobeSegment(Formats::ByteReader& segment)
    {
        if (segment.Size() < AdobeSegmentSize)
            return;

        if (segment.ReadText(AdobeSignature.size()) != AdobeSignature)
            return;

        segment.Skip(AdobeVersionAndFlagsSize);
        if (segment.ReadByte() == AdobeTransformNone)
            colors = JpegColors::RedGreenBlue;
    }

    void ReadScan()
    {
        if (!hasFrame)
            reader.Fail("a scan comes before the frame header");

        std::uint16_t headerLength = reader.ReadUint16();
        if (headerLength < SegmentLengthSize)
            reader.Fail("a scan header is shorter than its length field");

        Formats::ByteReader header(reader.ReadBytes(headerLength - SegmentLengthSize), DataDescription);
        JpegScan scan = ReadScanHeader(header);

        std::size_t dataStart = reader.Position();
        std::size_t dataEnd = FindEndOfScanData(dataStart);

        JpegBitReader bits(fileBytes.subspan(dataStart, dataEnd - dataStart));
        JpegScanDecoder decoder(frame, scan, frameHuffmanTables, restartInterval);
        decoder.Decode(bits);

        reader.MoveTo(dataEnd);
    }

    JpegScan ReadScanHeader(Formats::ByteReader& header)
    {
        JpegScan scan;
        int componentCount = header.ReadByte();
        for (int componentNumber = 0; componentNumber < componentCount; ++componentNumber)
        {
            int componentIndex = FindComponent(header.ReadByte(), header);
            Formats::Byte tables = header.ReadByte();
            JpegComponent& component = frame.components[componentIndex];
            component.dcTableIndex = HighNibble(tables);
            component.acTableIndex = LowNibble(tables);
            if (component.dcTableIndex >= HuffmanTableSlots || component.acTableIndex >= HuffmanTableSlots)
                header.Fail("a scan names a Huffman table that cannot exist");

            scan.componentIndexes.push_back(componentIndex);
        }

        scan.spectralStart = header.ReadByte();
        scan.spectralEnd = header.ReadByte();
        Formats::Byte approximation = header.ReadByte();
        scan.successiveHigh = HighNibble(approximation);
        scan.successiveLow = LowNibble(approximation);

        if (scan.componentIndexes.empty())
            header.Fail("a scan has no component");

        if (scan.spectralEnd >= CoefficientsPerBlock || scan.spectralStart > scan.spectralEnd)
            header.Fail("a scan has a band of coefficients that cannot exist");

        bool isBandOfOneComponent = scan.spectralStart == 0 || scan.componentIndexes.size() == 1;
        if (frame.isProgressive && !isBandOfOneComponent)
            header.Fail("a progressive scan of AC coefficients has more than one component");

        return scan;
    }

    int FindComponent(Formats::Byte id, Formats::ByteReader& header)
    {
        for (std::size_t index = 0; index < frame.components.size(); ++index)
        {
            if (frame.components[index].id == id)
                return static_cast<int>(index);
        }
        header.Fail(std::format("a scan names the component {}, which the frame does not have", id));
    }

    // The data of a scan ends at the first marker that is not a restart marker.
    std::size_t FindEndOfScanData(std::size_t start) const
    {
        for (std::size_t place = start; place + 1 < fileBytes.size(); ++place)
        {
            if (fileBytes[place] != MarkerPrefix)
                continue;

            Formats::Byte next = fileBytes[place + 1];
            if (next != StuffedZero && !IsRestartMarker(next) && next != MarkerPrefix)
                return place;
        }
        return fileBytes.size();
    }

    Formats::ByteView fileBytes;
    Formats::ByteReader reader;
    JpegFrame frame;
    bool hasFrame = false;
    bool hasReachedEnd = false;
    std::array<QuantizationTable, QuantizationTableSlots> quantizationTables{};
    JpegHuffmanTables frameHuffmanTables;
    int restartInterval = 0;
    JpegColors colors = JpegColors::BrightnessAndColorDifferences;
};

}

Formats::RgbaImage DecodeJpeg(Formats::ByteView bytes)
{
    JpegFileReader fileReader(bytes);
    return fileReader.Decode();
}

JpegInformation ReadJpegInformation(Formats::ByteView bytes)
{
    JpegFileReader fileReader(bytes);
    return fileReader.ReadUpToFrame();
}

}
