#include "Sound/SoundMode.h"

namespace Sound
{

SoundMode SoundModeFromSetting(std::optional<std::uint64_t> setting)
{
    if (!setting)
        return SoundMode::Stereo;

    switch (*setting)
    {
    case static_cast<std::uint64_t>(SoundMode::Mono): return SoundMode::Mono;
    case static_cast<std::uint64_t>(SoundMode::Surround): return SoundMode::Surround;
    default: return SoundMode::Stereo;
    }
}

}
