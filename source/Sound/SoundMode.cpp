#include "Sound/SoundMode.h"

namespace Sound
{

SoundMode SoundModeFromSetting(std::uint8_t setting)
{
    switch (setting)
    {
    case static_cast<std::uint8_t>(SoundMode::Mono): return SoundMode::Mono;
    case static_cast<std::uint8_t>(SoundMode::Surround): return SoundMode::Surround;
    default: return SoundMode::Stereo;
    }
}

}
