#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace Input
{

// The Wii Remote has four lights in a row. They show which player the remote is.
constexpr int PlayerLedCount = 4;
using PlayerLeds = std::array<bool, PlayerLedCount>;

// The HOME menu cuts a sound for the speaker of a remote into packets of 40 samples and sends one packet every 1/150 second, which makes
// 6000 samples in one second. The remote gets each packet as 20 bytes of 4 bit ADPCM.
// Likely (docs/audit/OpenPoints/Input.md, INP-01): the configuration of the speaker would mean 3000 Hz by the formula of WiiBrew.
constexpr std::uint32_t RemoteSpeakerSampleRate = 6000;

// A sound for the speaker of a remote: one channel, 16 bits for every sample. A real remote gets the samples encoded as ADPCM.
struct SpeakerSound
{
    // How many samples play in one second. The speakers of the remotes play RemoteSpeakerSampleRate.
    std::uint32_t sampleRate = RemoteSpeakerSampleRate;

    std::vector<std::int16_t> samples;
};

}
