#include "Media/JpegBitReader.h"

namespace Media
{

namespace
{

constexpr Formats::Byte MarkerPrefix = 0xFF;
constexpr Formats::Byte StuffedZero = 0x00;
constexpr Formats::Byte FirstRestartMarker = 0xD0;
constexpr Formats::Byte LastRestartMarker = 0xD7;
constexpr int BitsPerByte = 8;
constexpr std::size_t MarkerSize = 2;

}

JpegBitReader::JpegBitReader(Formats::ByteView scanData)
    : data(scanData)
{
}

int JpegBitReader::ReadBit()
{
    if (bitsLeftInByte == 0)
    {
        currentByte = ReadByte();
        bitsLeftInByte = BitsPerByte;
    }

    --bitsLeftInByte;
    return (currentByte >> bitsLeftInByte) & 1;
}

int JpegBitReader::ReadBits(int count)
{
    int number = 0;
    for (int bitNumber = 0; bitNumber < count; ++bitNumber)
        number = (number << 1) | ReadBit();

    return number;
}

int JpegBitReader::ReadSignedNumber(int count)
{
    if (count == 0)
        return 0;

    int number = ReadBits(count);
    int smallestPositive = 1 << (count - 1);
    if (number >= smallestPositive)
        return number;

    return number - (1 << count) + 1;
}

void JpegBitReader::Restart()
{
    bitsLeftInByte = 0;
    if (IsRestartMarkerAt(position))
        position += MarkerSize;
}

int JpegBitReader::ReadByte()
{
    if (position >= data.size())
        return 0;

    Formats::Byte value = data[position];
    if (value != MarkerPrefix)
    {
        ++position;
        return value;
    }

    bool isStuffed = position + 1 < data.size() && data[position + 1] == StuffedZero;
    if (!isStuffed)
        return 0;

    position += MarkerSize;
    return value;
}

bool JpegBitReader::IsRestartMarkerAt(std::size_t place) const
{
    if (place + 1 >= data.size())
        return false;

    if (data[place] != MarkerPrefix)
        return false;

    return data[place + 1] >= FirstRestartMarker && data[place + 1] <= LastRestartMarker;
}

}
