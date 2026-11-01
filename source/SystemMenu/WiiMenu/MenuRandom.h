#pragma once

#include <cstdint>

namespace SystemMenu
{

// The random numbers of the menu (the shared state 0x810890D4 of the original), a linear congruential generator.
// The televisions of the grid take the first frame of their animations from it, so that they do not flicker together.
class MenuRandom
{
public:
    explicit MenuRandom(std::uint32_t seed)
        : state(seed)
    {
    }

    // A number from 0 to 65535.
    std::uint32_t Next()
    {
        state = state * Multiplier + Increment;
        return state >> KeptBitsShift;
    }

private:
    static constexpr std::uint32_t Multiplier = 0x19660D;
    static constexpr std::uint32_t Increment = 0x3C6EF35F;
    static constexpr int KeptBitsShift = 16;

    std::uint32_t state;
};

}
