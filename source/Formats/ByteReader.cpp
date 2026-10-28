#include "Formats/ByteReader.h"

#include "Formats/FormatError.h"

#include <bit>
#include <format>

namespace Formats
{

ByteReader::ByteReader(ByteView bytesToRead, std::string_view dataDescription)
    : bytes(bytesToRead)
    , description(dataDescription)
{
}

std::size_t ByteReader::Size() const
{
    return bytes.size();
}

std::size_t ByteReader::Position() const
{
    return position;
}

std::size_t ByteReader::RemainingSize() const
{
    return bytes.size() - position;
}

bool ByteReader::IsAtEnd() const
{
    return position == bytes.size();
}

void ByteReader::MoveTo(std::size_t newPosition)
{
    if (newPosition > bytes.size())
    {
        throw FormatError(std::format("{}: cannot move to offset {:#x}, the data has only {:#x} bytes",
            description, newPosition, bytes.size()));
    }

    position = newPosition;
}

void ByteReader::Skip(std::size_t count)
{
    RequireBytes(count);
    position += count;
}

Byte ByteReader::ReadByte()
{
    return static_cast<Byte>(ReadBigEndianNumber(1));
}

std::uint16_t ByteReader::ReadUint16()
{
    return static_cast<std::uint16_t>(ReadBigEndianNumber(2));
}

std::uint32_t ByteReader::ReadUint32()
{
    return static_cast<std::uint32_t>(ReadBigEndianNumber(4));
}

std::uint64_t ByteReader::ReadUint64()
{
    return ReadBigEndianNumber(8);
}

std::int16_t ByteReader::ReadInt16()
{
    return static_cast<std::int16_t>(ReadUint16());
}

float ByteReader::ReadFloat()
{
    return std::bit_cast<float>(ReadUint32());
}

ByteView ByteReader::ReadBytes(std::size_t count)
{
    RequireBytes(count);
    ByteView readBytes = bytes.subspan(position, count);
    position += count;
    return readBytes;
}

std::string ByteReader::ReadText(std::size_t fieldSize)
{
    ByteView field = ReadBytes(fieldSize);

    std::string text;
    for (Byte character : field)
    {
        if (character == 0)
            break;

        text.push_back(static_cast<char>(character));
    }
    return text;
}

std::string ByteReader::ReadTextUntilZero()
{
    std::string text;
    while (true)
    {
        Byte character = ReadByte();
        if (character == 0)
            return text;

        text.push_back(static_cast<char>(character));
    }
}

void ByteReader::RequireBytes(std::size_t count) const
{
    if (count <= RemainingSize())
        return;

    throw FormatError(std::format("{}: cannot read {} bytes at offset {:#x}, only {} bytes are left",
        description, count, position, RemainingSize()));
}

std::uint64_t ByteReader::ReadBigEndianNumber(std::size_t byteCount)
{
    ByteView numberBytes = ReadBytes(byteCount);

    std::uint64_t number = 0;
    for (Byte numberByte : numberBytes)
        number = (number << 8) | numberByte;
    return number;
}

bool IsRangeInside(std::uint64_t offset, std::uint64_t size, std::uint64_t totalSize)
{
    if (offset > totalSize)
        return false;

    return size <= totalSize - offset;
}

}
