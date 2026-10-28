#pragma once

#include "SystemMenu/MenuConsole.h"
#include "SystemMenu/WiiMenu/SaveData.h"

#include <vector>

namespace SystemMenu
{

// Brings the places of the channel grid up to date with the titles that are installed, as the menu does when it starts
// (docs/audit/OpenPoints/SystemMenu.md SYS-04):
//  - Only titles that the grid shows are considered, and not the empty stand-ins of the system channels.
//  - Of the Photo Channel in its three versions exactly one is shown.
//  - A place keeps its channel when that is installed. When the channel was only the first version, the letter A, and a later
//    version of it is installed, the later version takes its place.
//  - A place whose channel is gone becomes empty.
//  - A title that has no place yet takes the first empty place, the channels of the menu's own table first.
void PlaceChannels(SaveData& saveData, const std::vector<InstalledTitle>& installedTitles);

}
