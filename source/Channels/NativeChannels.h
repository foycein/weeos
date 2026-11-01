#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"

#include "SystemMenu/TitleContents.h"

#include "Formats/Bytes.h"
#include "Ios/Title/TitleId.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Channels
{

// The channels that have a program in WeeOS.
constexpr Ios::TitleId WiiShopChannelTitleId = {0x00010002, 0x48414241};
constexpr Ios::TitleId ForecastChannelTitleId = {0x00010002, 0x48414645};
constexpr Ios::TitleId NewsChannelTitleId = {0x00010002, 0x48414745};
constexpr Ios::TitleId InternetChannelTitleId = {0x00010001, 0x48414445};

// The shared contents that the channel system reads for a channel: the two bitmap fonts of the console and the channel's copy of the
// HOME Button Menu, in the folder of the version that the channel's program opens.
struct ChannelSharedFiles
{
    std::uint16_t fontContentIndex;
    std::uint16_t homeMenuContentIndex;
    std::string_view homeMenuFolder;
    bool isHomeMenuPacked;

    // The big buttons of the HOME menu that WeeOS offers: "Wii Menu" and "Reset". The third one of HomeButton3 opens the channel's
    // manual, which is not built (CHN-13), so it is left out.
    std::size_t offeredHomeMenuButtons;
};

std::optional<ChannelSharedFiles> SharedFilesOf(Ios::TitleId titleId);

// The sound archives that a channel plays: its own and the one of its HOME menu, each read from a content of its title (CHN-30).
// A channel asks for its sounds by name (ChannelServices::sounds), and the program plays each name from the archive that has it.
std::vector<Formats::Bytes> ReadSoundArchives(Ios::TitleId titleId, SystemMenu::TitleContents& contents);

// True when WeeOS has a program for the title. The menu shows its message for a title that cannot start for the others.
bool HasNativeProgram(Ios::TitleId titleId);

// The program of the channel. Nothing when WeeOS has none for the title. The services must outlive the channel.
std::unique_ptr<Channel> MakeChannel(Ios::TitleId titleId, ChannelServices& services);

}
