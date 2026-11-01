#include "Settings/ConsoleClock.h"

namespace Settings
{

using namespace std::chrono;

CalendarTime ConsoleClock::ToCalendarTime(local_seconds time)
{
    local_days day = floor<days>(time);
    year_month_day date(day);
    hh_mm_ss<seconds> timeOfDay(time - day);

    CalendarTime calendarTime;
    calendarTime.year = static_cast<int>(date.year());
    calendarTime.month = static_cast<int>(static_cast<unsigned>(date.month()));
    calendarTime.day = static_cast<int>(static_cast<unsigned>(date.day()));
    calendarTime.hour = static_cast<int>(timeOfDay.hours().count());
    calendarTime.minute = static_cast<int>(timeOfDay.minutes().count());
    calendarTime.second = static_cast<int>(timeOfDay.seconds().count());
    return calendarTime;
}

local_seconds ConsoleClock::ToLocalSeconds(const CalendarTime& time)
{
    year_month_day date(std::chrono::year(time.year), std::chrono::month(static_cast<unsigned>(time.month)),
        std::chrono::day(static_cast<unsigned>(time.day)));
    return local_days(date) + hours(time.hour) + minutes(time.minute) + seconds(time.second);
}

// The clock of the PC in whole seconds, as the people around the PC read it.
local_seconds ConsoleClock::ReadWindowsClock()
{
    zoned_time<seconds> now(current_zone(), floor<seconds>(system_clock::now()));
    return now.get_local_time();
}

}
