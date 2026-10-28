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

}
