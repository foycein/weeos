#pragma once

#include "SystemMenu/WiiMenu/ChannelManager.h"
#include "SystemMenu/WiiMenu/MenuConsole.h"

#include <optional>

namespace SystemMenu
{

// What the Wii Menu keeps while the program runs, however often its screens are made and taken away again.
struct WiiMenuData
{
    // Reads the file of the channels. The console must outlive this object.
    explicit WiiMenuData(MenuConsole& menuConsole)
        : console(menuConsole)
        , channels(menuConsole)
    {
    }

    MenuConsole& console;
    ChannelManager channels;

    // The clock shows the words "Wii Menu" for a few seconds the first time the Wii Menu appears, and the time from then on.
    bool hasClockShownTheLabel = false;

    // The channel that was pressed last. The channel start window shows it.
    std::optional<SlotPosition> pressedChannel;
};

}
