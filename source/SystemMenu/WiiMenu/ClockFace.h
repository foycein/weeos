#pragma once

#include "SystemMenu/WiiMenu/MenuConsole.h"

#include "Text/MessageCatalog.h"

#include <string>

namespace SystemMenu
{

// What the clock of the menu shows for a time: four digits, and AM or PM for the hours of 12. A console of the USA shows the hours from
// 1 to 12 and leaves out the first digit of the hours when it is 0.
struct ClockFace
{
    int hourTens = 0;
    int hourOnes = 0;
    int minuteTens = 0;
    int minuteOnes = 0;
    bool isAfternoon = false;
    bool showsHourTens = false;
    bool showsMorningOrAfternoon = true;

    bool operator==(const ClockFace&) const = default;
};

ClockFace MakeClockFace(const LocalTime& time, const ClockFormat& format = ClockFormat());

// The same time of day, a number of days earlier (a negative number) or later.
LocalTime ShiftDays(const LocalTime& time, int days);

// The day of the week and the date as the menu writes them above the Message Board, for example "Wed 10/28" in English.
// The names of the days are messages of the menu.
std::u16string MakeDateText(const LocalTime& time, const Text::MessageCatalog& messages);

}
