#pragma once

#include "SystemMenu/MenuConsole.h"
#include "SystemMenu/WiiMenu/ChannelManager.h"
#include "SystemMenu/WiiMenu/MenuSettings.h"

namespace SystemMenu
{

// What the Wii Menu keeps while the program runs, however often its screens are made and taken away again.
struct WiiMenuData
{
    // Reads the settings and the file of the channels. The console must outlive this object.
    explicit WiiMenuData(MenuConsole& menuConsole)
        : console(menuConsole)
        , settings(ReadMenuSettings(menuConsole))
        , channels(menuConsole)
    {
    }

    MenuConsole& console;
    MenuSettings settings;
    ChannelManager channels;

    // The clock shows the words "Wii Menu" for a few seconds the first time the Wii Menu appears, and the time from then on.
    bool hasClockShownTheLabel = false;
};

}
