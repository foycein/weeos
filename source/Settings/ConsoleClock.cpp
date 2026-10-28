#include "Settings/ConsoleClock.h"

#include <utility>

namespace Settings
{

namespace
{

using namespace std::chrono;

// The clock of the PC in whole seconds, as the people around the PC read it.
local_seconds ReadLocalTimeOfWindows()
{
    zoned_time<seconds> now(current_zone(), floor<seconds>(system_clock::now()));
    return now.get_local_time();
}

}

ConsoleClock::ConsoleClock(ClockBiasSource clockBias, PcClock pcClock)
    : clockBias(std::move(clockBias))
    , pcClock(std::move(pcClock))
{
}

CalendarTime ConsoleClock::Now() const
{
    return ToCalendarTime(pcClock() + std::chrono::seconds(clockBias()));
}

std::int32_t ConsoleClock::BiasForTime(const CalendarTime& time) const
{
    std::chrono::seconds bias = ToLocalSeconds(time) - pcClock();
    return static_cast<std::int32_t>(bias.count());
}

int ConsoleClock::DaysInMonth(int year, int month)
{
    using namespace std::chrono;
    year_month_day_last lastDay(std::chrono::year(year) / std::chrono::month(static_cast<unsigned>(month)) / last);
    return static_cast<int>(unsigned(lastDay.day()));
}

CalendarTime ConsoleClock::ToCalendarTime(local_seconds time)
{
    using namespace std::chrono;
    local_days day = floor<days>(time);
    year_month_day date(day);
    hh_mm_ss<seconds> timeOfDay(time - day);

    CalendarTime calendarTime;
    calendarTime.year = int(date.year());
    calendarTime.month = static_cast<int>(unsigned(date.month()));
    calendarTime.day = static_cast<int>(unsigned(date.day()));
    calendarTime.hour = static_cast<int>(timeOfDay.hours().count());
    calendarTime.minute = static_cast<int>(timeOfDay.minutes().count());
    calendarTime.second = static_cast<int>(timeOfDay.seconds().count());
    return calendarTime;
}

local_seconds ConsoleClock::ToLocalSeconds(const CalendarTime& time)
{
    using namespace std::chrono;
    year_month_day date(std::chrono::year(time.year), std::chrono::month(static_cast<unsigned>(time.month)),
        std::chrono::day(static_cast<unsigned>(time.day)));
    return local_days(date) + hours(time.hour) + minutes(time.minute) + seconds(time.second);
}

local_seconds ConsoleClock::ReadWindowsClock()
{
    return ReadLocalTimeOfWindows();
}

}
