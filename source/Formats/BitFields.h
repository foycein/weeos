#pragma once

namespace Formats
{

// Takes bitCount bits out of the value. The lowest bit of the value is bit 0.
constexpr unsigned ExtractBits(unsigned value, int lowestBit, int bitCount)
{
    return (value >> lowestBit) & ((1u << bitCount) - 1);
}

}
