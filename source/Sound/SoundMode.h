#pragma once

#include <cstdint>
#include <optional>

namespace Sound
{

// How the console plays its sound. The user chooses it in the settings of the console, which store the number in the setting IPL.SND.
// Mono plays every sound in the middle. Stereo places a sound with its pan. Surround also places a sound in front of the
// listener or behind, which a stereo output can only show as a sound that is partly out of phase.
enum class SoundMode
{
    Mono = 0,
    Stereo = 1,
    Surround = 2,
};

// The mode for the number of the setting. A setting that is missing, or a number that is no mode, gives stereo, which is what the
// original does (0x8156a248).
SoundMode SoundModeFromSetting(std::optional<std::uint64_t> setting);

}
