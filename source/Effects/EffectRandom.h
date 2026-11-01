#pragma once

#include <cstdint>

namespace Effects
{

// The random numbers of the effect library: a linear congruential generator, of which only the upper 16 bits are used.
class EffectRandom
{
public:
    explicit EffectRandom(std::uint32_t seed = 0);

    // The next 16 random bits, 0 to 65535.
    std::uint32_t NextBits();

    // From 0 up to (not including) 1.
    float NextFraction();

    // From -32768 to 32767.
    std::int16_t NextSigned();

private:
    std::uint32_t state;
};

std::uint32_t NextRandomState(std::uint32_t state);

// The random number that a random key of an animation gives one particle: the same particle, animation, loop and key always give
// the same number.
std::uint32_t KeyRandomNumber(std::uint16_t particleSeed, std::uint16_t animationSeed, std::uint32_t loop, std::uint16_t keyValue);

}
