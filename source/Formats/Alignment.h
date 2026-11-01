#pragma once

#include <cstdint>

namespace Formats
{

constexpr std::uint64_t RoundUpToMultiple(std::uint64_t value, std::uint64_t multiple)
{
    std::uint64_t remainder = value % multiple;
    if (remainder == 0)
        return value;

    return value + multiple - remainder;
}

// How many pieces of the given size are needed to hold the value, for example the clusters of a file.
constexpr std::uint64_t DivideRoundingUp(std::uint64_t value, std::uint64_t pieceSize)
{
    return RoundUpToMultiple(value, pieceSize) / pieceSize;
}

}
