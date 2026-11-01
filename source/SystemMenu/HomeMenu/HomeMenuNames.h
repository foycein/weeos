#pragma once

#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

// The names of panes and groups of the layout th_HomeBtn_d that more than one part of the HOME menu uses.
namespace SystemMenu::HomeMenuNames
{

// The five buttons of the remote settings, in the order of the places Area::VolumeDown to Area::Reconnect: their groups start with
// these names, and their pointer areas are the panes of the second list.
constexpr std::size_t RemoteSettingsButtonCount = 5;
constexpr std::array<std::string_view, RemoteSettingsButtonCount> RemoteSettingsButtons = {
    "optnBtn_00", "optnBtn_01", "optnBtn_10", "optnBtn_11", "optnBtn_20"};
constexpr std::array<std::string_view, RemoteSettingsButtonCount> RemoteSettingsAreaPanes = {
    "B_optnBtn_00", "B_optnBtn_01", "B_optnBtn_10", "B_optnBtn_11", "B_optnBtn_20"};

// The ten bars of the volume.
inline std::string VolumeBarGroup(int bar)
{
    return std::format("vol_{:02}", bar);
}

// The battery of a player and its four bars.
inline std::string PlayerGroup(int player)
{
    return std::format("plyr_{:02}", player);
}

inline std::string BatteryBarPane(int player, int bar)
{
    return std::format("btryPwr_{:02}_{}", player, bar);
}

}
