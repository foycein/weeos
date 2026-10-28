#pragma once

#include "Formats/Bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Ios
{

// Writes numbers and texts into a buffer of bytes at a place, the way the Wii stores them: the most significant byte comes first.
// The buffer is made first, as long as the file, so every place is inside it. A place outside is a mistake in the program.

inline void PutByte(Formats::Bytes& bytes, std::size_t offset, Formats::Byte value)
{
    bytes.at(offset) = value;
}

inline void PutUint16(Formats::Bytes& bytes, std::size_t offset, std::uint16_t value)
{
    PutByte(bytes, offset, static_cast<Formats::Byte>(value >> Formats::BitsPerByte));
    PutByte(bytes, offset + 1, static_cast<Formats::Byte>(value));
}

inline void PutUint32(Formats::Bytes& bytes, std::size_t offset, std::uint32_t value)
{
    constexpr int bitsPerHalf = 2 * Formats::BitsPerByte;

    PutUint16(bytes, offset, static_cast<std::uint16_t>(value >> bitsPerHalf));
    PutUint16(bytes, offset + sizeof(std::uint16_t), static_cast<std::uint16_t>(value));
}

inline void PutUint64(Formats::Bytes& bytes, std::size_t offset, std::uint64_t value)
{
    constexpr int bitsPerUpperHalf = 4 * Formats::BitsPerByte;

    PutUint32(bytes, offset, static_cast<std::uint32_t>(value >> bitsPerUpperHalf));
    PutUint32(bytes, offset + sizeof(std::uint32_t), static_cast<std::uint32_t>(value));
}

inline void PutBytes(Formats::Bytes& bytes, std::size_t offset, Formats::ByteView data)
{
    if (offset + data.size() > bytes.size())
        throw std::out_of_range("Bytes were put outside of the buffer");

    std::copy(data.begin(), data.end(), bytes.begin() + offset);
}

// Puts the text into a field of a fixed size. The rest of the field stays as it is, which is zero in a new buffer.
inline void PutText(Formats::Bytes& bytes, std::size_t offset, std::string_view text, std::size_t fieldSize)
{
    if (text.size() > fieldSize)
        throw std::invalid_argument("A text is longer than its field");

    Formats::ByteView textBytes(reinterpret_cast<const Formats::Byte*>(text.data()), text.size());
    PutBytes(bytes, offset, textBytes);
}

}
