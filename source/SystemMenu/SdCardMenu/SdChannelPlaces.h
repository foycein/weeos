#pragma once

#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "SystemMenu/DataManagement/DataResult.h"

#include "Formats/SdCardBackup.h"

#include <cstddef>

namespace SystemMenu
{

// The channels of the SD card in the places where the SD Card Menu shows them (screen 0x22: 0x813DBE94, 0x813DBFE0; the worker
// reads and writes /private/wii/loc.dat and loc.bak at 0x8134BC60 and 0x8134B91C).
struct SdCardChannels
{
    Formats::ChannelPlaces places = {};

    // The places changed, or a file of the places was missing or damaged: the menu writes them again.
    bool mustBeWritten = false;

    // The card holds more channels than the menu has places.
    bool hasTooManyChannels = false;
};

// A channel keeps its place. A place whose channel is not on the card any more becomes empty, and a channel without a place takes
// the first empty one, in the order of the card. The Photo Channel 1.1 (HAZA) is never shown.
SdCardChannels PlaceChannelsOfCard(DataManagementServices& services);

// Writes loc.dat and loc.bak.
DataResult WriteChannelPlaces(DataManagementServices& services, const Formats::ChannelPlaces& places);

// A channel can only be put into an empty place. Gives back false and changes nothing when the place is taken or the first place is empty.
bool MoveChannelToPlace(Formats::ChannelPlaces& places, std::size_t fromPlace, std::size_t toPlace);

}
