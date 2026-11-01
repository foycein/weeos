#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>

namespace Formats
{

// Writes numbers the way the Wii stores them: the most significant byte comes first.
class ByteWriter
{
public:
    void WriteByte(Byte value);
    void WriteUint16(std::uint16_t value);
    void WriteUint32(std::uint32_t value);
    void WriteUint64(std::uint64_t value);
    void WriteFloat(float value);
    void WriteBytes(ByteView moreBytes);

    // Writes zero bytes.
    void WriteZeros(std::size_t count);

    // The bytes written so far.
    const Bytes& GetBytes() const;

private:
    void WriteBigEndianNumber(std::uint64_t number, std::size_t byteCount);

    Bytes bytes;
};

}
