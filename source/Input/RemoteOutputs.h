#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace Input
{

// The Wii Remote has four lights in a row. They show which player the remote is.
constexpr int PlayerLedCount = 4;
using PlayerLeds = std::array<bool, PlayerLedCount>;

// A sound for the speaker of a remote: one channel, 16 bits for every sample.
struct SpeakerSound
{
    // How many samples play in one second.
    std::uint32_t sampleRate = 0;

    std::vector<std::int16_t> samples;
};

}
