#include "SystemMenu/MessageBoard/BoardDays.h"

#include <chrono>

namespace SystemMenu
{

namespace
{

constexpr int SecondsPerMinute = 60;
constexpr int SecondsPerHour = 60 * SecondsPerMinute;
constexpr int SecondsPerDay = 24 * SecondsPerHour;

std::chrono::sys_days DaysOf(BoardDay day)
{
    std::chrono::year_month_day date(std::chrono::year(day.year), std::chrono::month(static_cast<unsigned>(day.month)),
        std::chrono::day(static_cast<unsigned>(day.day)));
    return std::chrono::sys_days(date);
}

BoardDay BoardDayOf(std::chrono::sys_days days)
{
    std::chrono::year_month_day date(days);
    return BoardDay{static_cast<int>(date.year()), static_cast<int>(static_cast<unsigned>(date.month())),
        static_cast<int>(static_cast<unsigned>(date.day()))};
}

int SecondsOfTheDay(const LocalTime& time)
{
    return time.hour * SecondsPerHour + time.minute * SecondsPerMinute + time.second;
}

}

BoardDay DayOf(const LocalTime& time)
{
    return BoardDay{time.year, time.month, time.day};
}

BoardDay ShiftDays(BoardDay day, int days)
{
    return BoardDayOf(DaysOf(day) + std::chrono::days(days));
}

int DaysBetween(BoardDay first, BoardDay second)
{
    return static_cast<int>((DaysOf(second) - DaysOf(first)).count());
}

int WeekdayOf(BoardDay day)
{
    return static_cast<int>(std::chrono::weekday(DaysOf(day)).c_encoding());
}

int DaysInMonth(int year, int month)
{
    std::chrono::year_month_day_last lastDay(std::chrono::year(year), std::chrono::month_day_last(std::chrono::month(static_cast<unsigned>(month))));
    return static_cast<int>(static_cast<unsigned>(lastDay.day()));
}

std::uint32_t SecondsSince2000(const LocalTime& time)
{
    BoardDay day = DayOf(time);
    if (day < FirstBoardDay)
        day = FirstBoardDay;
    if (day > LastBoardDay)
        day = LastBoardDay;

    std::int64_t days = DaysBetween(FirstBoardDay, day);
    return static_cast<std::uint32_t>(days * SecondsPerDay + SecondsOfTheDay(time));
}

LocalTime TimeOfSeconds(std::uint32_t secondsSince2000)
{
    BoardDay day = ShiftDays(FirstBoardDay, static_cast<int>(secondsSince2000 / SecondsPerDay));
    int secondOfTheDay = static_cast<int>(secondsSince2000 % SecondsPerDay);

    LocalTime time;
    time.year = day.year;
    time.month = day.month;
    time.day = day.day;
    time.hour = secondOfTheDay / SecondsPerHour;
    time.minute = secondOfTheDay % SecondsPerHour / SecondsPerMinute;
    time.second = secondOfTheDay % SecondsPerMinute;
    return time;
}

std::uint64_t TicksOf(const LocalTime& time)
{
    return std::uint64_t{SecondsSince2000(time)} * ConsoleTicksPerSecond;
}

LocalTime TimeOfTicks(std::uint64_t ticks)
{
    return TimeOfSeconds(static_cast<std::uint32_t>(ticks / ConsoleTicksPerSecond));
}

LocalTime AtTimeOfDay(BoardDay day, const LocalTime& timeOfDay)
{
    LocalTime time = timeOfDay;
    time.year = day.year;
    time.month = day.month;
    time.day = day.day;
    return time;
}

std::array<CalendarCell, CalendarCellCount> MakeCalendarPage(int year, int month)
{
    BoardDay firstOfTheMonth{year, month, 1};
    BoardDay firstOfThePage = ShiftDays(firstOfTheMonth, -WeekdayOf(firstOfTheMonth));

    std::array<CalendarCell, CalendarCellCount> page;
    for (int index = 0; index < CalendarCellCount; ++index)
    {
        BoardDay day = ShiftDays(firstOfThePage, index);
        page[index] = CalendarCell{day, day.year == year && day.month == month};
    }
    return page;
}

}
