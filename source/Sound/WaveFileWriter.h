#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace Sound
{

// The bytes of a WAV file with 32 bit floating point samples. The samples of the channels alternate:
// left, right, left, right and so on.
Formats::Bytes MakeFloatWaveFileBytes(std::uint32_t sampleRate, std::uint16_t channelCount, std::span<const float> interleavedSamples);

// Writes such a file. Throws std::runtime_error when the file cannot be written.
void WriteFloatWaveFile(const std::filesystem::path& path, std::uint32_t sampleRate, std::uint16_t channelCount,
    std::span<const float> interleavedSamples);

}
