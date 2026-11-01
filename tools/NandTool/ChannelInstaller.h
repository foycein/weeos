#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// A system channel that the reference console does not have, and the WAD of it that tools/fetch_titles.py put into assets.
struct ChannelTheDumpLacks
{
    std::string_view name;
    std::string_view wadInAssets;
};

// The reference console never had the Wii Speak Channel (CHN-19); the owner's channel list has it (docs/DECISIONS.md).
constexpr ChannelTheDumpLacks ChannelsTheDumpLacks[] = {
    {"Wii Speak Channel 2.0", "0001000148434645-v512/title.wad"},
};

// Installs these channels into an imported NAND folder through the title service of IOS, with the identity of the dump, the way the
// console installs a WAD (CHN-35). The assets folder is the folder that holds the dump. Gives one line of report for each channel.
std::vector<std::string> InstallChannelsTheDumpLacks(const std::filesystem::path& nandFolder, const std::filesystem::path& dumpFolder);
