#pragma once

#include "SystemMenu/WiiMenu/MenuConsole.h"

#include "Formats/BannerContent.h"
#include "Formats/PlayRecordFile.h"
#include "Formats/U8Archive.h"
#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace SystemMenu
{

// What the channel start window needs to know about a channel before it starts it (docs/research/SystemMenuStructure.md 4.5).

// The title as the title service lists it. Nothing when it is not installed.
std::optional<InstalledTitle> FindInstalledTitle(MenuConsole& console, Ios::TitleId titleId);

// The animations that a banner plays: its start once, and then its loop.
struct BannerAnimations
{
    std::optional<std::string> start;
    std::optional<std::string> loop;

    // The loop is the first animation of the banner's script, which plays on the group of the same name.
    bool loopsOnTheScriptGroup = false;
};

// The group of a banner that the first animation of its script plays on.
constexpr std::string_view ScriptGroupOfBanners = "Rso0";

// A banner plays banner_Start once when it has it (0x813B83D8), then banner_Loop, or banner when it has no banner_Loop. A banner that a
// script or a module drives loops the first animation of the script, banner_Rso0, while the channel has no data of its own (SYS-92).
// The files are the files of banner.bin.
BannerAnimations ChooseBannerAnimations(const Formats::U8Archive& bannerFiles, bool isBannerDrivenByCode);

// The ticks of the timer of the console (a quarter of the bus clock of 243 MHz) since the start of the year 2000, at this time.
std::uint64_t TimerTicksAt(const LocalTime& time);

// The record of the title that starts now (0x81536DC4): the two lines of its name, the time of the start as the first and the last time,
// the game code (the low half of the title id) and the maker code (the group of its title metadata, "01" for Nintendo).
Formats::PlayRecord MakePlayRecord(const Formats::ChannelName& name, Ios::TitleId titleId, std::uint16_t makerCode, const LocalTime& time);

}
