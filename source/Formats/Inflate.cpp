#include "Formats/Inflate.h"

#include "Formats/FormatError.h"

#include <array>
#include <cstdint>
#include <format>
#include <vector>

namespace Formats
{

namespace
{

constexpr std::size_t ZlibHeaderSize = 2;
constexpr Byte DeflateMethod = 8;
constexpr Byte MethodMask = 0x0F;
constexpr Byte PresetDictionaryFlag = 0x20;

constexpr int StoredBlock = 0;
constexpr int FixedCodeBlock = 1;
constexpr int DynamicCodeBlock = 2;

constexpr int MaximumCodeLength = 15;
constexpr int EndOfBlockSymbol = 256;
constexpr int FirstLengthSymbol = 257;

// The symbols 257 to 285 stand for a length. Each starts at a base length and is followed by some extra bits.
constexpr std::array<std::uint16_t, 29> LengthBases = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};
constexpr std::array<Byte, 29> LengthExtraBits = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};

// The symbols 0 to 29 stand for a distance, the same way.
constexpr std::array<std::uint16_t, 30> DistanceBases = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};
constexpr std::array<Byte, 30> DistanceExtraBits = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

// A block with dynamic codes starts with the lengths of the codes of the lengths. They come in this order.
constexpr std::array<Byte, 19> LengthCodeOrder = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
constexpr int RepeatPreviousLength = 16;
constexpr int RepeatZeroShort = 17;

constexpr int FixedLiteralCodeCount = 288;
constexpr int FixedDistanceCodeCount = 30;

[[noreturn]] void Fail(const std::string& problem)
{
    throw FormatError(std::format("Deflate data: {}", problem));
}

// Reads bits starting with the lowest bit of each byte, as deflate does.
class BitReader
{
public:
    explicit BitReader(ByteView bytesToRead)
        : bytes(bytesToRead)
    {
    }

    std::uint32_t ReadBits(int count)
    {
        std::uint32_t value = 0;
        for (int bitNumber = 0; bitNumber < count; ++bitNumber)
            value |= ReadBit() << bitNumber;

        return value;
    }

    std::uint32_t ReadBit()
    {
        if (bytePosition >= bytes.size())
            Fail("the data ends too early");

        std::uint32_t bit = (bytes[bytePosition] >> bitPosition) & 1;
        ++bitPosition;
        if (bitPosition == BitsPerByte)
        {
            bitPosition = 0;
            ++bytePosition;
        }

        return bit;
    }

    // Skips the rest of the byte that was started, as a stored block does.
    void SkipToNextByte()
    {
        if (bitPosition != 0)
        {
            bitPosition = 0;
            ++bytePosition;
        }
    }

    ByteView ReadBytes(std::size_t count)
    {
        if (bytePosition + count > bytes.size())
            Fail("a stored block is longer than the data");

        ByteView readBytes = bytes.subspan(bytePosition, count);
        bytePosition += count;
        return readBytes;
    }

private:
    ByteView bytes;
    std::size_t bytePosition = 0;
    int bitPosition = 0;
};

// A set of codes that is told by the number of codes of each length. The codes of one length follow each other
// and the shorter codes come first, so a code is found by counting.
struct CodeTable
{
    std::array<std::uint16_t, MaximumCodeLength + 1> countOfLength = {};

    // The symbols, ordered by the length of their codes and then by their number.
    std::vector<std::uint16_t> symbols;
};

CodeTable MakeCodeTable(const std::vector<Byte>& lengthOfSymbol)
{
    CodeTable table;
    for (Byte length : lengthOfSymbol)
        ++table.countOfLength[length];

    // Symbols without a code have the length 0.
    table.countOfLength[0] = 0;

    std::array<std::uint16_t, MaximumCodeLength + 2> offsetOfLength = {};
    for (int length = 1; length <= MaximumCodeLength; ++length)
        offsetOfLength[length + 1] = static_cast<std::uint16_t>(offsetOfLength[length] + table.countOfLength[length]);

    table.symbols.resize(lengthOfSymbol.size());
    for (std::size_t symbol = 0; symbol < lengthOfSymbol.size(); ++symbol)
    {
        Byte length = lengthOfSymbol[symbol];
        if (length != 0)
            table.symbols[offsetOfLength[length]++] = static_cast<std::uint16_t>(symbol);
    }

    return table;
}

int DecodeSymbol(BitReader& reader, const CodeTable& table)
{
    int code = 0;
    int firstCodeOfLength = 0;
    int indexOfFirstSymbol = 0;

    for (int length = 1; length <= MaximumCodeLength; ++length)
    {
        code |= static_cast<int>(reader.ReadBit());

        int count = table.countOfLength[length];
        if (code - count < firstCodeOfLength)
            return table.symbols[indexOfFirstSymbol + (code - firstCodeOfLength)];

        indexOfFirstSymbol += count;
        firstCodeOfLength += count;
        firstCodeOfLength <<= 1;
        code <<= 1;
    }

    Fail("a code that does not exist");
}

void InflateStoredBlock(BitReader& reader, Bytes& output)
{
    reader.SkipToNextByte();
    std::uint32_t length = reader.ReadBits(16);
    std::uint32_t lengthComplement = reader.ReadBits(16);
    if ((length ^ lengthComplement) != 0xFFFF)
        Fail("a stored block with a wrong length");

    ByteView data = reader.ReadBytes(length);
    output.insert(output.end(), data.begin(), data.end());
}

void CopyEarlierBytes(Bytes& output, std::size_t distance, std::size_t length)
{
    if (distance > output.size())
        Fail("a distance that reaches before the start");

    // The bytes are copied one at a time, because the copy may overlap its source.
    for (std::size_t copied = 0; copied < length; ++copied)
        output.push_back(output[output.size() - distance]);
}

void InflateBlockData(BitReader& reader, const CodeTable& literalTable, const CodeTable& distanceTable, Bytes& output)
{
    while (true)
    {
        int symbol = DecodeSymbol(reader, literalTable);
        if (symbol < EndOfBlockSymbol)
        {
            output.push_back(static_cast<Byte>(symbol));
            continue;
        }

        if (symbol == EndOfBlockSymbol)
            return;

        std::size_t lengthIndex = static_cast<std::size_t>(symbol - FirstLengthSymbol);
        if (lengthIndex >= LengthBases.size())
            Fail("a length symbol that does not exist");

        std::size_t length = LengthBases[lengthIndex] + reader.ReadBits(LengthExtraBits[lengthIndex]);

        std::size_t distanceIndex = static_cast<std::size_t>(DecodeSymbol(reader, distanceTable));
        if (distanceIndex >= DistanceBases.size())
            Fail("a distance symbol that does not exist");

        std::size_t distance = DistanceBases[distanceIndex] + reader.ReadBits(DistanceExtraBits[distanceIndex]);
        CopyEarlierBytes(output, distance, length);
    }
}

// The codes of a block with fixed codes: literals and lengths have 8 to 9 bits, distances have 5.
void InflateFixedCodeBlock(BitReader& reader, Bytes& output)
{
    std::vector<Byte> literalLengths(FixedLiteralCodeCount);
    for (int symbol = 0; symbol < FixedLiteralCodeCount; ++symbol)
    {
        if (symbol < 144)
            literalLengths[symbol] = 8;
        else if (symbol < 256)
            literalLengths[symbol] = 9;
        else if (symbol < 280)
            literalLengths[symbol] = 7;
        else
            literalLengths[symbol] = 8;
    }

    std::vector<Byte> distanceLengths(FixedDistanceCodeCount, 5);
    InflateBlockData(reader, MakeCodeTable(literalLengths), MakeCodeTable(distanceLengths), output);
}

std::vector<Byte> ReadLengthsOfCodes(BitReader& reader, const CodeTable& lengthTable, int count)
{
    std::vector<Byte> lengths;
    while (static_cast<int>(lengths.size()) < count)
    {
        int symbol = DecodeSymbol(reader, lengthTable);
        if (symbol < RepeatPreviousLength)
        {
            lengths.push_back(static_cast<Byte>(symbol));
            continue;
        }

        Byte repeatedLength = 0;
        std::uint32_t repeatCount = 0;
        if (symbol == RepeatPreviousLength)
        {
            if (lengths.empty())
                Fail("a repeat of a length that does not exist");

            repeatedLength = lengths.back();
            repeatCount = 3 + reader.ReadBits(2);
        }
        else if (symbol == RepeatZeroShort)
        {
            repeatCount = 3 + reader.ReadBits(3);
        }
        else
        {
            repeatCount = 11 + reader.ReadBits(7);
        }

        if (lengths.size() + repeatCount > static_cast<std::size_t>(count))
            Fail("more lengths of codes than the block says");

        lengths.insert(lengths.end(), repeatCount, repeatedLength);
    }

    return lengths;
}

// A block with dynamic codes brings its own codes: first the codes for the lengths of the codes, then the literal and distance codes.
void InflateDynamicCodeBlock(BitReader& reader, Bytes& output)
{
    int literalCodeCount = static_cast<int>(reader.ReadBits(5)) + 257;
    int distanceCodeCount = static_cast<int>(reader.ReadBits(5)) + 1;
    int lengthCodeCount = static_cast<int>(reader.ReadBits(4)) + 4;

    std::vector<Byte> lengthCodeLengths(LengthCodeOrder.size(), 0);
    for (int index = 0; index < lengthCodeCount; ++index)
        lengthCodeLengths[LengthCodeOrder[index]] = static_cast<Byte>(reader.ReadBits(3));

    CodeTable lengthTable = MakeCodeTable(lengthCodeLengths);
    std::vector<Byte> allLengths = ReadLengthsOfCodes(reader, lengthTable, literalCodeCount + distanceCodeCount);

    std::vector<Byte> literalLengths(allLengths.begin(), allLengths.begin() + literalCodeCount);
    std::vector<Byte> distanceLengths(allLengths.begin() + literalCodeCount, allLengths.end());
    InflateBlockData(reader, MakeCodeTable(literalLengths), MakeCodeTable(distanceLengths), output);
}

void CheckZlibHeader(ByteView zlibBytes)
{
    if (zlibBytes.size() < ZlibHeaderSize)
        Fail("the zlib header is missing");

    if ((zlibBytes[0] & MethodMask) != DeflateMethod)
        Fail("the zlib header names a method that is not deflate");

    if ((zlibBytes[1] & PresetDictionaryFlag) != 0)
        Fail("the zlib header asks for a dictionary");
}

}

Bytes Inflate(ByteView deflateBytes)
{
    BitReader reader(deflateBytes);
    Bytes output;
    bool isLastBlock = false;
    while (!isLastBlock)
    {
        isLastBlock = reader.ReadBit() != 0;
        std::uint32_t blockType = reader.ReadBits(2);

        if (blockType == StoredBlock)
            InflateStoredBlock(reader, output);
        else if (blockType == FixedCodeBlock)
            InflateFixedCodeBlock(reader, output);
        else if (blockType == DynamicCodeBlock)
            InflateDynamicCodeBlock(reader, output);
        else
            Fail("a block of a kind that does not exist");
    }

    return output;
}

Bytes InflateZlib(ByteView zlibBytes)
{
    CheckZlibHeader(zlibBytes);
    return Inflate(zlibBytes.subspan(ZlibHeaderSize));
}

}
