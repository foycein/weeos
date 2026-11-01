#include "Media/JpegEncoder.h"

#include "Media/JpegFrame.h"
#include "Media/JpegHuffmanTable.h"

#include "Formats/ByteWriter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string_view>

namespace Media
{

namespace
{

constexpr Formats::Byte MarkerPrefix = 0xFF;
constexpr Formats::Byte StartOfImage = 0xD8;
constexpr Formats::Byte EndOfImage = 0xD9;
constexpr Formats::Byte JfifSegment = 0xE0;
constexpr Formats::Byte DefineQuantizationTables = 0xDB;
constexpr Formats::Byte BaselineFrame = 0xC0;
constexpr Formats::Byte DefineHuffmanTables = 0xC4;
constexpr Formats::Byte StartOfScan = 0xDA;
constexpr Formats::Byte StuffedZero = 0x00;

// The quantization tables of Annex K (K.1), row by row.
constexpr std::array<int, CoefficientsPerBlock> LuminanceQuantization = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};
constexpr std::array<int, CoefficientsPerBlock> ChrominanceQuantization = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

// The quality scales the tables as the IJG library does: below 50 by 5000 / quality percent, from 50 on by 200 - 2 * quality percent.
constexpr int HighestQuality = 100;
constexpr int MiddleQuality = 50;
constexpr int LargestQuantization = 255;
constexpr int LowQualityScale = 5000;
constexpr int HighQualityScaleStart = 200;
constexpr int HighQualityScaleStep = 2;

constexpr float LevelShift = 128;
constexpr int MacroblockSide = 2 * BlockSide;
constexpr int SamplePrecision = 8;
constexpr int SegmentLengthSize = 2;
constexpr int ComponentCount = 3;
constexpr Formats::Byte LuminanceId = 1;
constexpr Formats::Byte BlueId = 2;
constexpr Formats::Byte RedId = 3;
constexpr Formats::Byte LuminanceSampling = 0x22;
constexpr Formats::Byte ColorSampling = 0x11;
constexpr int LuminanceSlot = 0;
constexpr int ChrominanceSlot = 1;
constexpr int DcClass = 0;
constexpr int AcClass = 1;
constexpr int NibbleBits = 4;
constexpr int EndOfBlock = 0x00;
constexpr int SixteenZeros = 0xF0;
constexpr int LongestZeroRun = 15;
constexpr int ByteValueCount = 256;

// The one scan holds every coefficient of the three components, with no successive approximation.
constexpr Formats::Byte FirstCoefficient = 0;
constexpr Formats::Byte LastCoefficient = CoefficientsPerBlock - 1;
constexpr Formats::Byte NoSuccessiveApproximation = 0;
constexpr Formats::Byte LuminanceTables = (LuminanceSlot << NibbleBits) | LuminanceSlot;
constexpr Formats::Byte ChrominanceTables = (ChrominanceSlot << NibbleBits) | ChrominanceSlot;

// JFIF's conversion from RGB to YCbCr.
constexpr float BrightnessFromRed = 0.299f;
constexpr float BrightnessFromGreen = 0.587f;
constexpr float BrightnessFromBlue = 0.114f;
constexpr float BlueDifferenceFromRed = -0.168736f;
constexpr float BlueDifferenceFromGreen = -0.331264f;
constexpr float BlueDifferenceFromBlue = 0.5f;
constexpr float RedDifferenceFromRed = 0.5f;
constexpr float RedDifferenceFromGreen = -0.418688f;
constexpr float RedDifferenceFromBlue = -0.081312f;

// The JFIF segment: its name, version 1.1, no unit, a pixel shape of 1:1, no thumbnail.
constexpr std::string_view JfifName = std::string_view("JFIF\0", 5);
constexpr std::array<Formats::Byte, 9> JfifFields = {1, 1, 0, 0, 1, 0, 1, 0, 0};

using QuantizationValues = std::array<int, CoefficientsPerBlock>;

QuantizationValues ScaledQuantization(const std::array<int, CoefficientsPerBlock>& table, int quality)
{
    int limitedQuality = std::clamp(quality, 1, HighestQuality);
    int scale = HighQualityScaleStart - HighQualityScaleStep * limitedQuality;
    if (limitedQuality < MiddleQuality)
        scale = LowQualityScale / limitedQuality;

    QuantizationValues scaled{};
    for (int index = 0; index < CoefficientsPerBlock; ++index)
        scaled[index] = std::clamp((table[index] * scale + MiddleQuality) / HighestQuality, 1, LargestQuantization);

    return scaled;
}

// The code and its length for every value of a Huffman table, made the way the decoder reads them (Annex C).
struct HuffmanCode
{
    int code = 0;
    int length = 0;
};

using HuffmanCodes = std::array<HuffmanCode, ByteValueCount>;

HuffmanCodes MakeCodes(const HuffmanTableDefinition& definition)
{
    HuffmanCodes codes{};
    int code = 0;
    std::size_t valueIndex = 0;
    for (int length = 1; length <= LongestHuffmanCode; ++length)
    {
        for (int count = 0; count < definition.codeCountOfLength[length - 1]; ++count)
        {
            Formats::Byte value = definition.values[valueIndex];
            codes[value] = {code, length};
            ++valueIndex;
            ++code;
        }

        code *= 2;
    }
    return codes;
}

// Writes the entropy coded data: the most significant bit first, a 0x00 after every 0xFF, the last byte filled with ones.
class JpegBitWriter
{
public:
    void WriteBits(int value, int count)
    {
        for (int bit = count - 1; bit >= 0; --bit)
        {
            currentByte = (currentByte << 1) | ((value >> bit) & 1);
            ++bitsInByte;
            if (bitsInByte == Formats::BitsPerByte)
                FlushByte();
        }
    }

    void WriteCode(const HuffmanCode& code)
    {
        WriteBits(code.code, code.length);
    }

    Formats::Bytes Finish()
    {
        while (bitsInByte != 0)
            WriteBits(1, 1);

        return bytes;
    }

private:
    void FlushByte()
    {
        bytes.push_back(static_cast<Formats::Byte>(currentByte));
        if (currentByte == MarkerPrefix)
            bytes.push_back(StuffedZero);

        currentByte = 0;
        bitsInByte = 0;
    }

    Formats::Bytes bytes;
    int currentByte = 0;
    int bitsInByte = 0;
};

// The number of bits of a value and the bits themselves, negative values as JPEG stores them (the value minus one, F.1.2.1).
int BitCountOf(int value)
{
    int magnitude = std::abs(value);
    int count = 0;
    while (magnitude > 0)
    {
        magnitude >>= 1;
        ++count;
    }
    return count;
}

int BitsOf(int value, int bitCount)
{
    if (value >= 0)
        return value;

    return value + (1 << bitCount) - 1;
}

// The forward discrete cosine transform of section A.3.3: F(u, v) = 1/4 C(u) C(v) sum f(x, y) cos(...) cos(...).
std::array<float, CoefficientsPerBlock> ForwardDct(const std::array<float, CoefficientsPerBlock>& samples)
{
    std::array<std::array<float, BlockSide>, BlockSide> cosines{};
    for (int frequency = 0; frequency < BlockSide; ++frequency)
    {
        double scale = frequency == 0 ? 1.0 / std::numbers::sqrt2 : 1.0;
        for (int sample = 0; sample < BlockSide; ++sample)
        {
            double angle = (2 * sample + 1) * frequency * std::numbers::pi / (2 * BlockSide);
            cosines[frequency][sample] = static_cast<float>(scale / 2 * std::cos(angle));
        }
    }

    std::array<float, CoefficientsPerBlock> rowsDone{};
    for (int row = 0; row < BlockSide; ++row)
    {
        for (int frequency = 0; frequency < BlockSide; ++frequency)
        {
            float sum = 0;
            for (int column = 0; column < BlockSide; ++column)
                sum += cosines[frequency][column] * samples[row * BlockSide + column];
            rowsDone[row * BlockSide + frequency] = sum;
        }
    }

    std::array<float, CoefficientsPerBlock> coefficients{};
    for (int column = 0; column < BlockSide; ++column)
    {
        for (int frequency = 0; frequency < BlockSide; ++frequency)
        {
            float sum = 0;
            for (int row = 0; row < BlockSide; ++row)
                sum += cosines[frequency][row] * rowsDone[row * BlockSide + column];
            coefficients[frequency * BlockSide + column] = sum;
        }
    }
    return coefficients;
}

struct Plane
{
    int width = 0;
    int height = 0;
    std::vector<float> samples;

    // A place outside the plane gives the nearest sample at its edge.
    float At(int x, int y) const
    {
        int column = std::clamp(x, 0, width - 1);
        int row = std::clamp(y, 0, height - 1);
        return samples[static_cast<std::size_t>(row) * width + column];
    }
};

struct ColorPlanes
{
    Plane brightness;
    Plane blue;
    Plane red;
};

Plane EmptyPlane(int width, int height)
{
    return {width, height, std::vector<float>(static_cast<std::size_t>(width) * height)};
}

float BrightnessOf(const Formats::Rgba8Pixel& pixel)
{
    return BrightnessFromRed * pixel.red + BrightnessFromGreen * pixel.green + BrightnessFromBlue * pixel.blue;
}

float BlueDifferenceOf(const Formats::Rgba8Pixel& pixel)
{
    return BlueDifferenceFromRed * pixel.red + BlueDifferenceFromGreen * pixel.green + BlueDifferenceFromBlue * pixel.blue + LevelShift;
}

float RedDifferenceOf(const Formats::Rgba8Pixel& pixel)
{
    return RedDifferenceFromRed * pixel.red + RedDifferenceFromGreen * pixel.green + RedDifferenceFromBlue * pixel.blue + LevelShift;
}

// The average of the 2 x 2 samples that one sample of the half plane covers.
float AverageOfFour(const Plane& plane, int halfX, int halfY)
{
    int left = 2 * halfX;
    int top = 2 * halfY;
    float sum = plane.At(left, top) + plane.At(left + 1, top) + plane.At(left, top + 1) + plane.At(left + 1, top + 1);
    return sum / 4;
}

Plane HalvePlane(const Plane& plane)
{
    int halfWidth = (plane.width + 1) / 2;
    int halfHeight = (plane.height + 1) / 2;
    Plane half = EmptyPlane(halfWidth, halfHeight);
    for (int y = 0; y < halfHeight; ++y)
    {
        for (int x = 0; x < halfWidth; ++x)
            half.samples[static_cast<std::size_t>(y) * halfWidth + x] = AverageOfFour(plane, x, y);
    }
    return half;
}

// The colors are kept at half the size in both directions.
ColorPlanes MakePlanes(const Formats::RgbaImage& picture)
{
    int width = static_cast<int>(picture.width);
    int height = static_cast<int>(picture.height);
    Plane brightness = EmptyPlane(width, height);
    Plane fullBlue = EmptyPlane(width, height);
    Plane fullRed = EmptyPlane(width, height);
    for (std::size_t index = 0; index < picture.pixels.size(); ++index)
    {
        const Formats::Rgba8Pixel& pixel = picture.pixels[index];
        brightness.samples[index] = BrightnessOf(pixel);
        fullBlue.samples[index] = BlueDifferenceOf(pixel);
        fullRed.samples[index] = RedDifferenceOf(pixel);
    }

    return {brightness, HalvePlane(fullBlue), HalvePlane(fullRed)};
}

class BlockEncoder
{
public:
    BlockEncoder(JpegBitWriter& writer, const QuantizationValues& table, const HuffmanCodes& dc, const HuffmanCodes& ac)
        : bits(writer)
        , quantization(table)
        , dcCodes(dc)
        , acCodes(ac)
    {
    }

    void Encode(const Plane& plane, int left, int top)
    {
        std::array<float, CoefficientsPerBlock> samples{};
        for (int y = 0; y < BlockSide; ++y)
        {
            for (int x = 0; x < BlockSide; ++x)
                samples[y * BlockSide + x] = plane.At(left + x, top + y) - LevelShift;
        }

        std::array<float, CoefficientsPerBlock> coefficients = ForwardDct(samples);
        std::array<int, CoefficientsPerBlock> quantized{};
        for (int zigZagPosition = 0; zigZagPosition < CoefficientsPerBlock; ++zigZagPosition)
        {
            int place = ZigZagToRowByRow[zigZagPosition];
            quantized[zigZagPosition] = static_cast<int>(std::lround(coefficients[place] / quantization[place]));
        }

        WriteAverage(quantized[0]);
        WriteBand(quantized);
    }

private:
    void WriteAverage(int average)
    {
        int difference = average - previousAverage;
        previousAverage = average;
        int bitCount = BitCountOf(difference);
        bits.WriteCode(dcCodes[bitCount]);
        bits.WriteBits(BitsOf(difference, bitCount), bitCount);
    }

    void WriteBand(const std::array<int, CoefficientsPerBlock>& quantized)
    {
        int zeroRun = 0;
        for (int zigZagPosition = 1; zigZagPosition < CoefficientsPerBlock; ++zigZagPosition)
        {
            int value = quantized[zigZagPosition];
            if (value == 0)
            {
                ++zeroRun;
                continue;
            }

            while (zeroRun > LongestZeroRun)
            {
                bits.WriteCode(acCodes[SixteenZeros]);
                zeroRun -= LongestZeroRun + 1;
            }

            int bitCount = BitCountOf(value);
            int runAndSize = (zeroRun << NibbleBits) | bitCount;
            bits.WriteCode(acCodes[runAndSize]);
            bits.WriteBits(BitsOf(value, bitCount), bitCount);
            zeroRun = 0;
        }

        if (zeroRun > 0)
            bits.WriteCode(acCodes[EndOfBlock]);
    }

    JpegBitWriter& bits;
    const QuantizationValues& quantization;
    const HuffmanCodes& dcCodes;
    const HuffmanCodes& acCodes;
    int previousAverage = 0;
};

void WriteSegment(Formats::ByteWriter& file, Formats::Byte marker, const Formats::Bytes& content)
{
    file.WriteByte(MarkerPrefix);
    file.WriteByte(marker);
    file.WriteUint16(static_cast<std::uint16_t>(content.size() + SegmentLengthSize));
    file.WriteBytes(content);
}

// The first byte holds the precision (0, 8 bits) in its high four bits and the slot in its low four bits.
void WriteQuantizationTable(Formats::ByteWriter& content, int slot, const QuantizationValues& table)
{
    content.WriteByte(static_cast<Formats::Byte>(slot));
    for (int zigZagPosition = 0; zigZagPosition < CoefficientsPerBlock; ++zigZagPosition)
        content.WriteByte(static_cast<Formats::Byte>(table[ZigZagToRowByRow[zigZagPosition]]));
}

Formats::Bytes QuantizationSegment(const QuantizationValues& luminance, const QuantizationValues& chrominance)
{
    Formats::ByteWriter content;
    WriteQuantizationTable(content, LuminanceSlot, luminance);
    WriteQuantizationTable(content, ChrominanceSlot, chrominance);
    return content.GetBytes();
}

void WriteHuffmanTable(Formats::ByteWriter& content, int tableClass, int slot, const HuffmanTableDefinition& definition)
{
    content.WriteByte(static_cast<Formats::Byte>((tableClass << NibbleBits) | slot));
    for (int count : definition.codeCountOfLength)
        content.WriteByte(static_cast<Formats::Byte>(count));
    content.WriteBytes(definition.values);
}

Formats::Bytes FrameSegment(const Formats::RgbaImage& picture)
{
    Formats::ByteWriter content;
    content.WriteByte(SamplePrecision);
    content.WriteUint16(static_cast<std::uint16_t>(picture.height));
    content.WriteUint16(static_cast<std::uint16_t>(picture.width));
    content.WriteByte(ComponentCount);
    for (Formats::Byte id : {LuminanceId, BlueId, RedId})
    {
        bool isLuminance = id == LuminanceId;
        content.WriteByte(id);
        content.WriteByte(isLuminance ? LuminanceSampling : ColorSampling);
        content.WriteByte(static_cast<Formats::Byte>(isLuminance ? LuminanceSlot : ChrominanceSlot));
    }
    return content.GetBytes();
}

Formats::Bytes ScanSegment()
{
    Formats::ByteWriter content;
    content.WriteByte(ComponentCount);
    content.WriteByte(LuminanceId);
    content.WriteByte(LuminanceTables);
    content.WriteByte(BlueId);
    content.WriteByte(ChrominanceTables);
    content.WriteByte(RedId);
    content.WriteByte(ChrominanceTables);
    content.WriteByte(FirstCoefficient);
    content.WriteByte(LastCoefficient);
    content.WriteByte(NoSuccessiveApproximation);
    return content.GetBytes();
}

Formats::Bytes JfifSegmentContent()
{
    Formats::ByteWriter jfif;
    jfif.WriteBytes(Formats::ByteView(reinterpret_cast<const Formats::Byte*>(JfifName.data()), JfifName.size()));
    jfif.WriteBytes(JfifFields);
    return jfif.GetBytes();
}

// The quantization and Huffman tables of the brightness and of the two colors.
struct EncodingTables
{
    QuantizationValues luminance{};
    QuantizationValues chrominance{};
    HuffmanTableDefinition luminanceDc = StandardLuminanceDcTable();
    HuffmanTableDefinition luminanceAc = StandardLuminanceAcTable();
    HuffmanTableDefinition chrominanceDc = StandardChrominanceDcTable();
    HuffmanTableDefinition chrominanceAc = StandardChrominanceAcTable();
};

Formats::Bytes HuffmanSegment(const EncodingTables& tables)
{
    Formats::ByteWriter content;
    WriteHuffmanTable(content, DcClass, LuminanceSlot, tables.luminanceDc);
    WriteHuffmanTable(content, AcClass, LuminanceSlot, tables.luminanceAc);
    WriteHuffmanTable(content, DcClass, ChrominanceSlot, tables.chrominanceDc);
    WriteHuffmanTable(content, AcClass, ChrominanceSlot, tables.chrominanceAc);
    return content.GetBytes();
}

// The blocks go out macroblock by macroblock: four of the brightness, then one of each color.
Formats::Bytes EncodeScanData(const Formats::RgbaImage& picture, const EncodingTables& tables)
{
    HuffmanCodes luminanceDcCodes = MakeCodes(tables.luminanceDc);
    HuffmanCodes luminanceAcCodes = MakeCodes(tables.luminanceAc);
    HuffmanCodes chrominanceDcCodes = MakeCodes(tables.chrominanceDc);
    HuffmanCodes chrominanceAcCodes = MakeCodes(tables.chrominanceAc);

    ColorPlanes planes = MakePlanes(picture);
    JpegBitWriter bits;
    BlockEncoder brightness(bits, tables.luminance, luminanceDcCodes, luminanceAcCodes);
    BlockEncoder blue(bits, tables.chrominance, chrominanceDcCodes, chrominanceAcCodes);
    BlockEncoder red(bits, tables.chrominance, chrominanceDcCodes, chrominanceAcCodes);
    for (int top = 0; top < static_cast<int>(picture.height); top += MacroblockSide)
    {
        for (int left = 0; left < static_cast<int>(picture.width); left += MacroblockSide)
        {
            brightness.Encode(planes.brightness, left, top);
            brightness.Encode(planes.brightness, left + BlockSide, top);
            brightness.Encode(planes.brightness, left, top + BlockSide);
            brightness.Encode(planes.brightness, left + BlockSide, top + BlockSide);
            blue.Encode(planes.blue, left / 2, top / 2);
            red.Encode(planes.red, left / 2, top / 2);
        }
    }
    return bits.Finish();
}

}

Formats::Bytes EncodeJpeg(const Formats::RgbaImage& picture, int quality)
{
    EncodingTables tables;
    tables.luminance = ScaledQuantization(LuminanceQuantization, quality);
    tables.chrominance = ScaledQuantization(ChrominanceQuantization, quality);

    Formats::ByteWriter file;
    file.WriteByte(MarkerPrefix);
    file.WriteByte(StartOfImage);
    WriteSegment(file, JfifSegment, JfifSegmentContent());
    WriteSegment(file, DefineQuantizationTables, QuantizationSegment(tables.luminance, tables.chrominance));
    WriteSegment(file, BaselineFrame, FrameSegment(picture));
    WriteSegment(file, DefineHuffmanTables, HuffmanSegment(tables));
    WriteSegment(file, StartOfScan, ScanSegment());

    file.WriteBytes(EncodeScanData(picture, tables));
    file.WriteByte(MarkerPrefix);
    file.WriteByte(EndOfImage);
    return file.GetBytes();
}

}
