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
    return static_cast<Byte>(ReadBigEndianNumber(sizeof(Byte)));
}

std::uint16_t ByteReader::ReadUint16()
{
    return static_cast<std::uint16_t>(ReadBigEndianNumber(sizeof(std::uint16_t)));
}

std::uint32_t ByteReader::ReadUint32()
{
    return static_cast<std::uint32_t>(ReadBigEndianNumber(sizeof(std::uint32_t)));
}

std::uint64_t ByteReader::ReadUint64()
{
    return ReadBigEndianNumber(sizeof(std::uint64_t));
}

std::int16_t ByteReader::ReadInt16()
{
    return static_cast<std::int16_t>(ReadUint16());
}

float ByteReader::ReadFloat()
{
    return std::bit_cast<float>(ReadUint32());
}

std::uint16_t ByteReader::ReadLittleEndianUint16()
{
    return static_cast<std::uint16_t>(ReadLittleEndianNumber(sizeof(std::uint16_t)));
}

std::uint32_t ByteReader::ReadLittleEndianUint32()
{
    return static_cast<std::uint32_t>(ReadLittleEndianNumber(sizeof(std::uint32_t)));
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

void ByteReader::Fail(std::string_view problem) const
{
    throw FormatError(std::format("{}: {} (at offset {:#x})", description, problem, position));
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
        number = (number << BitsPerByte) | numberByte;
    return number;
}

std::uint64_t ByteReader::ReadLittleEndianNumber(std::size_t byteCount)
{
    ByteView numberBytes = ReadBytes(byteCount);

    std::uint64_t number = 0;
    for (std::size_t byteIndex = byteCount; byteIndex > 0; --byteIndex)
        number = (number << BitsPerByte) | numberBytes[byteIndex - 1];

    return number;
}

bool IsRangeInside(std::uint64_t offset, std::uint64_t size, std::uint64_t totalSize)
{
    if (offset > totalSize)
        return false;

    return size <= totalSize - offset;
}

}
