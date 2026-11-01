#pragma once

#include "Formats/Bytes.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The names of the sound effects that the screens of Wii Options ask for, as they are named in the sound archive IplSound.brsar
// (docs/research/SystemMenuStructure.md 6.5).
namespace SoundName
{

constexpr std::string_view ButtonTargeted = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view Decide = "WIPL_SE_DECIDE";
constexpr std::string_view Cancel = "WIPL_SE_CANCEL";

}

// The sounds that the screens want to hear in one step of the menu. The menu has no sound player yet: the player will take
// the names from here after every step and play them.
class SoundRequests
{
public:
    void Request(std::string_view soundName)
    {
        names.emplace_back(soundName);
    }

    // The sounds requested since the last call of StartNextStep, in the order in which they were requested.
    const std::vector<std::string>& RequestedInThisStep() const
    {
        return names;
    }

    // The sound of a channel's banner, its file meta/sound.bin, which the channel start window plays while it shows the banner
    // (0x8136C198). The player decodes it with Sound::ReadBannerSound. It replaces a banner sound that still plays.
    void StartBannerSound(Formats::ByteView soundFile)
    {
        bannerSoundToStart = Formats::Bytes(soundFile.begin(), soundFile.end());
    }

    // The banner sound fades out (0x8136BA68).
    void StopBannerSound()
    {
        bannerSoundToStart.reset();
        isBannerSoundToStop = true;
    }

    const std::optional<Formats::Bytes>& BannerSoundToStart() const
    {
        return bannerSoundToStart;
    }

    bool IsBannerSoundToStop() const
    {
        return isBannerSoundToStop;
    }

    void StartNextStep()
    {
        names.clear();
        bannerSoundToStart.reset();
        isBannerSoundToStop = false;
    }

private:
    std::vector<std::string> names;
    std::optional<Formats::Bytes> bannerSoundToStart;
    bool isBannerSoundToStop = false;
};

}
