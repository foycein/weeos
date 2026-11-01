#include "Formats/Lz77.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::uint32_t Tag = 0x4C5A3737;
constexpr Byte SupportedType = 0x10;
constexpr std::size_t SizeFieldByteCount = 3;
constexpr std::size_t ShortestMatchLength = 3;
constexpr std::size_t ShortestDistance = 1;
constexpr int LengthBitShift = 4;
constexpr unsigned DistanceHighBitsMask = 0x0F;
constexpr int FlagsPerByte = 8;
constexpr unsigned HighestFlagBit = 0x80;

// The size of the result is stored in three bytes, the least significant byte first.
std::size_t ReadOutputSize(ByteReader& reader)
{
    std::size_t size = 0;
    for (std::size_t byteIndex = 0; byteIndex < SizeFieldByteCount; ++byteIndex)
        size |= std::size_t(reader.ReadByte()) << (BitsPerByte * byteIndex);
    return size;
}

// A match is two bytes. The first holds the length minus 3 in its high four bits and the top four bits
// of the distance minus 1 in its low four bits. The second byte holds the other eight bits of the distance.
void CopyMatch(ByteReader& reader, Bytes& output, std::size_t outputSize)
{
    Byte first = reader.ReadByte();
    Byte second = reader.ReadByte();

    std::size_t length = (first >> LengthBitShift) + ShortestMatchLength;
    std::size_t storedDistance = ((first & DistanceHighBitsMask) << BitsPerByte) | second;
    std::size_t distance = storedDistance + ShortestDistance;
    if (distance > output.size())
        reader.Fail("a match copies from before the start of the data");

    for (std::size_t copied = 0; copied < length && output.size() < outputSize; ++copied)
        output.push_back(output[output.size() - distance]);
}

}

bool HasLz77Tag(ByteView bytes)
{
    if (bytes.size() < sizeof(Tag))
        return false;

    ByteReader reader(bytes, "LZ77 data");
    return reader.ReadUint32() == Tag;
}

// The data is a row of groups. Each group starts with a flags byte. Its bits, the highest first, tell for
// the next eight items whether the item is a single byte that is copied or a match.
Bytes DecompressLz77(ByteView compressedBytes)
{
    ByteReader reader(compressedBytes, "LZ77 data");
    if (HasLz77Tag(compressedBytes))
        reader.Skip(sizeof(Tag));

    Byte type = reader.ReadByte();
    if (type != SupportedType)
        reader.Fail(std::format("the compression type {:#x} is not supported, only {:#x} is", type, SupportedType));

    std::size_t outputSize = ReadOutputSize(reader);

    Bytes output;
    output.reserve(outputSize);
    while (output.size() < outputSize)
    {
        Byte flags = reader.ReadByte();

        for (int item = 0; item < FlagsPerByte && output.size() < outputSize; ++item)
        {
            bool isMatch = (flags & (HighestFlagBit >> item)) != 0;
            if (isMatch)
                CopyMatch(reader, output, outputSize);
            else
                output.push_back(reader.ReadByte());
        }
    }

    return output;
}

}
