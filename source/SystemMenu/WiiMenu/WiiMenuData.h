#pragma once

#include "SystemMenu/WiiMenu/ChannelManager.h"
#include "SystemMenu/WiiMenu/MenuConsole.h"
#include "SystemMenu/WiiMenu/MenuRandom.h"

#include <optional>

namespace SystemMenu
{

inline std::uint32_t SecondsOfTheDay(const LocalTime& time)
{
    constexpr int SecondsPerMinute = 60;
    constexpr int MinutesPerHour = 60;
    return static_cast<std::uint32_t>((time.hour * MinutesPerHour + time.minute) * SecondsPerMinute + time.second);
}

// What the Wii Menu keeps while the program runs, however often its screens are made and taken away again.
struct WiiMenuData
{
    // Reads the file of the channels. The console must outlive this object.
    explicit WiiMenuData(MenuConsole& menuConsole)
        : console(menuConsole)
        , channels(menuConsole)
        , random(SecondsOfTheDay(menuConsole.GetLocalTime()))
    {
    }

    MenuConsole& console;
    ChannelManager channels;

    // Seeded with the time of the start, as the original seeds its random numbers with the time.
    MenuRandom random;

    // The clock shows the words "Wii Menu" for a few seconds the first time the Wii Menu appears, and the time from then on.
    bool hasClockShownTheLabel = false;

    // The music of the menu starts once, when the grid appears for the first time.
    bool hasMusicStarted = false;

    // The channel that was pressed last. The channel start window shows it.
    std::optional<SlotPosition> pressedChannel;
};

}
