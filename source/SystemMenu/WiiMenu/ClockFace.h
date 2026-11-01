#pragma once

#include "SystemMenu/MenuConsole.h"

#include "Text/MessageCatalog.h"

#include <string>

namespace SystemMenu
{

// What the clock of the menu shows for a time: four digits and AM or PM. A console of the USA shows the hours from 1 to 12,
// and the first digit of the hours is left out when it is 0.
struct ClockFace
{
    int hourTens = 0;
    int hourOnes = 0;
    int minuteTens = 0;
    int minuteOnes = 0;
    bool isAfternoon = false;

    bool operator==(const ClockFace&) const = default;
};

ClockFace MakeClockFace(const LocalTime& time);

// The day of the week and the date as the menu writes them above the Message Board, for example "Wed 10/28" in English.
// The names of the days are messages of the menu.
std::u16string MakeDateText(const LocalTime& time, const Text::MessageCatalog& messages);

}
