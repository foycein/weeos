#include "Formats/Yaz0.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

namespace Formats
{

namespace
{

constexpr std::uint32_t Yaz0Tag = 0x59617A30;
constexpr std::size_t HeaderSize = 16;
constexpr std::size_t SizeOffset = 4;

// A group byte announces eight items, the highest bit first: 1 is one byte to copy, 0 is a copy of earlier output.
constexpr int ItemsPerGroup = 8;
constexpr unsigned FirstItemBit = 0x80;

// A copy names its distance back in 12 bits and its length in 4 bits; length 0 means that a third byte holds the length minus 18.
constexpr unsigned DistanceHighMask = 0x0F;
constexpr int LengthShift = 4;
constexpr std::size_t ShortLengthAddition = 2;
constexpr std::size_t LongLengthAddition = 0x12;

}

bool IsYaz0Compressed(ByteView bytes)
{
    if (bytes.size() < sizeof(std::uint32_t))
        return false;

    ByteReader reader(bytes, "Yaz0 data");
    return reader.ReadUint32() == Yaz0Tag;
}

Bytes DecompressYaz0(ByteView compressed)
{
    ByteReader reader(compressed, "Yaz0 data");
    if (reader.ReadUint32() != Yaz0Tag)
        reader.Fail("the data does not start with the tag Yaz0");

    reader.MoveTo(SizeOffset);
    std::size_t unpackedSize = reader.ReadUint32();
    reader.MoveTo(HeaderSize);

    Bytes output;
    output.reserve(unpackedSize);

    while (output.size() < unpackedSize)
    {
        Byte group = reader.ReadByte();
        for (int item = 0; item < ItemsPerGroup && output.size() < unpackedSize; ++item)
        {
            bool isLiteral = (group & (FirstItemBit >> item)) != 0;
            if (isLiteral)
            {
                output.push_back(reader.ReadByte());
                continue;
            }

            Byte first = reader.ReadByte();
            Byte second = reader.ReadByte();
            std::size_t distance = (((first & DistanceHighMask) << BitsPerByte) | second) + 1;
            std::size_t length = (first >> LengthShift) + ShortLengthAddition;
            if ((first >> LengthShift) == 0)
                length = reader.ReadByte() + LongLengthAddition;

            if (distance > output.size())
                reader.Fail("a copy reaches back before the start of the output");

            std::size_t start = output.size() - distance;
            for (std::size_t index = 0; index < length; ++index)
                output.push_back(output[start + index]);
        }
    }

    output.resize(unpackedSize);
    return output;
}

}
