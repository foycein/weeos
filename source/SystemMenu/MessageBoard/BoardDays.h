#pragma once

#include "SystemMenu/WiiMenu/MenuConsole.h"

#include <array>
#include <compare>
#include <cstdint>

namespace SystemMenu
{

// A day of the Message Board.
struct BoardDay
{
    int year = 2000;
    int month = 1;
    int day = 1;

    auto operator<=>(const BoardDay&) const = default;
};

// The board and the calendar go from the first day of 2000 to the last day of 2035 (0x8139104C, 0x81391064, 0x8139F870).
constexpr BoardDay FirstBoardDay{2000, 1, 1};
constexpr BoardDay LastBoardDay{2035, 12, 31};

// The timer of the console counts a quarter of the bus clock of 243 MHz. Its ticks are the time of the messages.
constexpr std::uint64_t ConsoleTicksPerSecond = 60'750'000;

constexpr int DaysPerWeek = 7;
constexpr int MonthsPerYear = 12;

BoardDay DayOf(const LocalTime& time);

// The same day a number of days later (or earlier, for a negative number).
BoardDay ShiftDays(BoardDay day, int days);

// How many days lie from the first day to the second; negative when the second comes first.
int DaysBetween(BoardDay first, BoardDay second);

// 0 is Sunday, 6 is Saturday.
int WeekdayOf(BoardDay day);

int DaysInMonth(int year, int month);

// The time of the CDB records: seconds since the start of the year 2000, local time. A time outside 2000 to 2035 is moved to the
// same time of the nearest day of these years, as the database does (0x81486230).
std::uint32_t SecondsSince2000(const LocalTime& time);
LocalTime TimeOfSeconds(std::uint32_t secondsSince2000);

// The time of the console's timer, in ticks since the start of 2000, and back.
std::uint64_t TicksOf(const LocalTime& time);
LocalTime TimeOfTicks(std::uint64_t ticks);

// The same day at another time.
LocalTime AtTimeOfDay(BoardDay day, const LocalTime& timeOfDay);

// One page of the calendar: six weeks of seven days, Sunday first, as a console of the USA shows it (0x8139DBB0). The days before and
// after the month fill the page.
struct CalendarCell
{
    BoardDay day;
    bool isInTheMonth = false;
};

constexpr int CalendarWeekCount = 6;
constexpr int CalendarCellCount = CalendarWeekCount * DaysPerWeek;

std::array<CalendarCell, CalendarCellCount> MakeCalendarPage(int year, int month);

}
