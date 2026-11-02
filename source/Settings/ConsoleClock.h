#pragma once

#include <chrono>
#include <functional>

namespace Settings
{

// A date and a time of day.
struct CalendarTime
{
    int year = 2000;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;

    bool operator==(const CalendarTime&) const = default;
};

// The clock of the PC, as the time of day on the wall of the PC, in whole seconds.
using PcClock = std::function<std::chrono::local_seconds()>;

// The time of day of the console. A Wii has a clock chip of its own that the user sets; WeeOS has no such setting: the console's time is
// always the clock of the PC.
class ConsoleClock
{
public:
    static CalendarTime ToCalendarTime(std::chrono::local_seconds time);
    static std::chrono::local_seconds ToLocalSeconds(const CalendarTime& time);

    static std::chrono::local_seconds ReadWindowsClock();
};

}
