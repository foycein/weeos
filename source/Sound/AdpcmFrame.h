#pragma once

#include <cstdint>

namespace Sound
{

// DSP-ADPCM stores the samples in frames of 8 bytes. The first byte of a frame is its header (the predictor and the scale),
// the other 7 bytes hold 14 samples of 4 bits each.
constexpr std::uint32_t AdpcmBytesPerFrame = 8;
constexpr std::uint32_t AdpcmSamplesPerFrame = 14;
constexpr std::uint32_t AdpcmNibblesPerFrame = 16;
constexpr std::uint32_t AdpcmHeaderNibbleCount = 2;

// An address in half bytes (nibbles) from the start of the data. The two nibbles of a frame header have no sample.
constexpr bool IsAdpcmSampleAddress(std::uint32_t nibbleAddress)
{
    return nibbleAddress % AdpcmNibblesPerFrame >= AdpcmHeaderNibbleCount;
}

constexpr std::uint32_t AdpcmSampleOfNibbleAddress(std::uint32_t nibbleAddress)
{
    std::uint32_t frameNumber = nibbleAddress / AdpcmNibblesPerFrame;
    std::uint32_t nibbleInFrame = nibbleAddress % AdpcmNibblesPerFrame;
    return frameNumber * AdpcmSamplesPerFrame + (nibbleInFrame - AdpcmHeaderNibbleCount);
}

constexpr std::uint32_t AdpcmFrameCount(std::uint32_t sampleCount)
{
    return (sampleCount + AdpcmSamplesPerFrame - 1) / AdpcmSamplesPerFrame;
}

constexpr std::uint32_t AdpcmByteCount(std::uint32_t sampleCount)
{
    return AdpcmFrameCount(sampleCount) * AdpcmBytesPerFrame;
}

}
