#include "Formats/ByteWriter.h"

#include <bit>

namespace Formats
{

void ByteWriter::WriteByte(Byte value)
{
    bytes.push_back(value);
}

void ByteWriter::WriteUint16(std::uint16_t value)
{
    WriteBigEndianNumber(value, sizeof(value));
}

void ByteWriter::WriteUint32(std::uint32_t value)
{
    WriteBigEndianNumber(value, sizeof(value));
}

void ByteWriter::WriteUint64(std::uint64_t value)
{
    WriteBigEndianNumber(value, sizeof(value));
}

void ByteWriter::WriteFloat(float value)
{
    WriteUint32(std::bit_cast<std::uint32_t>(value));
}

void ByteWriter::WriteBytes(ByteView moreBytes)
{
    bytes.insert(bytes.end(), moreBytes.begin(), moreBytes.end());
}

void ByteWriter::WriteZeros(std::size_t count)
{
    bytes.insert(bytes.end(), count, Byte(0));
}

const Bytes& ByteWriter::GetBytes() const
{
    return bytes;
}

void ByteWriter::WriteBigEndianNumber(std::uint64_t number, std::size_t byteCount)
{
    for (std::size_t index = byteCount; index > 0; --index)
    {
        int shift = static_cast<int>((index - 1) * BitsPerByte);
        bytes.push_back(static_cast<Byte>((number >> shift) & ByteMask));
    }
}

}
