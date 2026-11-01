#include "Effects/EffectRandom.h"

namespace Effects
{

namespace
{

constexpr std::uint32_t Multiplier = 0x343FD;
constexpr std::uint32_t Increment = 0x269EC3;
constexpr int UpperBitsShift = 16;
constexpr float NumbersOf16Bits = 65536;

constexpr std::uint32_t ParticleFactor = 0x3F81F635;
constexpr std::uint32_t AnimationFactor = 0x30A74193;
constexpr std::uint32_t LoopFactor = 0x7B929;
constexpr std::uint32_t KeyFactor = 0x371097E7;
constexpr std::uint32_t KeyIncrement = 0x4BF53;
constexpr std::uint32_t ByteMask = 0xFF;

std::uint32_t ByteOf(std::uint32_t number, int byteNumber)
{
    return (number >> (byteNumber * 8)) & ByteMask;
}

}

std::uint32_t NextRandomState(std::uint32_t state)
{
    return state * Multiplier + Increment;
}

EffectRandom::EffectRandom(std::uint32_t seed)
    : state(seed)
{
}

std::uint32_t EffectRandom::NextBits()
{
    state = NextRandomState(state);
    return state >> UpperBitsShift;
}

float EffectRandom::NextFraction()
{
    return static_cast<float>(NextBits()) / NumbersOf16Bits;
}

std::int16_t EffectRandom::NextSigned()
{
    return static_cast<std::int16_t>(NextBits());
}

// Each byte is mixed with the one below it, from the second lowest up to the highest.
std::uint32_t KeyRandomNumber(std::uint16_t particleSeed, std::uint16_t animationSeed, std::uint32_t loop, std::uint16_t keyValue)
{
    std::uint32_t number = particleSeed * ParticleFactor + animationSeed * AnimationFactor + loop * LoopFactor + keyValue * KeyFactor +
        KeyIncrement;

    std::uint32_t lowest = ByteOf(number, 0);
    std::uint32_t second = ByteOf(number, 1) ^ lowest;
    std::uint32_t third = ByteOf(number, 2) ^ second;
    std::uint32_t highest = ByteOf(number, 3) ^ third;
    return lowest | (second << 8) | (third << 16) | (highest << 24);
}

}
