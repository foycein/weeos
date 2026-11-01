#include "SystemMenu/WiiMenu/ClockFace.h"

#include "Text/TextFormat.h"

#include <array>
#include <chrono>

namespace SystemMenu
{

namespace
{

constexpr int HoursOnTheClock = 12;
constexpr int NoonHour = 12;
constexpr int NumberBase = 10;

// The message numbers of the names of the days, starting with Sunday: Sun, Mon, Tue, Wed, Thu, Fri, Sat. The order of the messages
// in the file is Monday to Saturday and then Sunday.
constexpr std::array<std::uint16_t, 7> DayNameMessages = {116, 110, 111, 112, 113, 114, 115};

std::chrono::sys_days DayOf(const LocalTime& time)
{
    std::chrono::year_month_day date(std::chrono::year(time.year), std::chrono::month(static_cast<unsigned>(time.month)),
        std::chrono::day(static_cast<unsigned>(time.day)));
    return std::chrono::sys_days(date);
}

int WeekdayNumberOf(const LocalTime& time)
{
    return static_cast<int>(std::chrono::weekday(DayOf(time)).c_encoding());
}

}

ClockFace MakeClockFace(const LocalTime& time)
{
    int hour = time.hour % HoursOnTheClock;
    if (hour == 0)
        hour = HoursOnTheClock;

    ClockFace face;
    face.hourTens = hour / NumberBase;
    face.hourOnes = hour % NumberBase;
    face.minuteTens = time.minute / NumberBase;
    face.minuteOnes = time.minute % NumberBase;
    face.isAfternoon = time.hour >= NoonHour;
    return face;
}

LocalTime ShiftDays(const LocalTime& time, int days)
{
    std::chrono::year_month_day date(DayOf(time) + std::chrono::days(days));

    LocalTime shifted = time;
    shifted.year = static_cast<int>(date.year());
    shifted.month = static_cast<int>(static_cast<unsigned>(date.month()));
    shifted.day = static_cast<int>(static_cast<unsigned>(date.day()));
    return shifted;
}

std::u16string MakeDateText(const LocalTime& time, const Text::MessageCatalog& messages)
{
    const std::u16string& dayName = messages.Find(DayNameMessages[WeekdayNumberOf(time)]);
    return Text::FormatText(u"%ls %d/%d", {dayName, time.month, time.day});
}

}
