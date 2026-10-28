#pragma once

#include <cstdint>

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

// The mode for the number of the setting. A number that is no mode gives stereo, which is what the original does.
SoundMode SoundModeFromSetting(std::uint8_t setting);

}
