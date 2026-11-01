#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

namespace Settings
{

// A date and a time of day, as the Wii Settings pages show them.
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

// The years that the console can show: it counts from the year 2000 and its menu stops at the end of 2035.
constexpr int FirstYearOfTheConsole = 2000;
constexpr int LastYearOfTheConsole = 2035;

// The clock of the PC, as the time of day on the wall of the PC, in whole seconds.
using PcClock = std::function<std::chrono::local_seconds()>;

// The seconds that are added to the clock of the PC to get the time of the console. The Wii Settings pages change it.
using ClockBiasSource = std::function<std::int32_t()>;

// The time of day of the console. The console has a clock chip of its own and a bias that the user can change; WeeOS has the clock of the PC,
// so the console's time is the clock of the PC plus the clock bias of the settings.
class ConsoleClock
{
public:
    // The bias is asked for every time the time is needed. The clock of the PC is the clock of Windows unless a test gives another.
    explicit ConsoleClock(ClockBiasSource clockBias, PcClock pcClock = ReadWindowsClock);

    CalendarTime Now() const;

    // The clock bias that makes the console show this time now. The bias is in seconds and fits 32 bits.
    std::int32_t BiasForTime(const CalendarTime& time) const;

    // The number of days in the month of the year, with leap years.
    static int DaysInMonth(int year, int month);

    // The time of the console as a day of the calendar and the seconds since midnight.
    static CalendarTime ToCalendarTime(std::chrono::local_seconds time);
    static std::chrono::local_seconds ToLocalSeconds(const CalendarTime& time);

    static std::chrono::local_seconds ReadWindowsClock();

private:
    ClockBiasSource clockBias;
    PcClock pcClock;
};

}
