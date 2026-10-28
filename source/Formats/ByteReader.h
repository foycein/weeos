#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Formats
{

// Reads numbers the way the Wii stores them: the most significant byte comes first.
// Every read is checked. Reading past the end throws a FormatError that names the data being read.
class ByteReader
{
public:
    ByteReader(ByteView bytesToRead, std::string_view dataDescription);

    std::size_t Size() const;
    std::size_t Position() const;
    std::size_t RemainingSize() const;
    bool IsAtEnd() const;

    void MoveTo(std::size_t newPosition);
    void Skip(std::size_t count);

    Byte ReadByte();
    std::uint16_t ReadUint16();
    std::uint32_t ReadUint32();
    std::uint64_t ReadUint64();
    std::int16_t ReadInt16();
    float ReadFloat();
    ByteView ReadBytes(std::size_t count);

    // Reads a text field of a fixed size. The text ends at the first zero byte.
    std::string ReadText(std::size_t fieldSize);

    // Reads text up to and including the next zero byte.
    std::string ReadTextUntilZero();

private:
    void RequireBytes(std::size_t count) const;
    std::uint64_t ReadBigEndianNumber(std::size_t byteCount);

    ByteView bytes;
    std::string description;
    std::size_t position = 0;
};

// True when the range [offset, offset + size) lies inside totalSize bytes. Safe for huge values.
bool IsRangeInside(std::uint64_t offset, std::uint64_t size, std::uint64_t totalSize);

}
