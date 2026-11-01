#include "SystemMenu/WiiSettings/CalendarPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"
#include "SystemMenu/WiiSettings/NumberText.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr int MonthsInYear = 12;
constexpr int HoursInDay = 24;
constexpr int MinutesInHour = 60;
constexpr int MinimumDigitsOfDateAndTime = 2;
constexpr int DigitsOfYear = 4;

// The arrows above and below the numbers, and the numbers between them, in points of the page.
constexpr int UpArrowTop = 108;
constexpr int DownArrowTop = 253;
constexpr int NumberTop = 180;
constexpr int NumberHeight = 72;
constexpr int SeparatorWidth = 48;

constexpr int MonthArrowLeft = 88;
constexpr int DayArrowLeft = 224;
constexpr int YearArrowLeft = 400;
constexpr PageRectangle MonthNumberBounds = {88, NumberTop, 88, NumberHeight};
constexpr PageRectangle FirstSlashBounds = {176, NumberTop, SeparatorWidth, NumberHeight};
constexpr PageRectangle DayNumberBounds = {224, NumberTop, 88, NumberHeight};
constexpr PageRectangle SecondSlashBounds = {312, NumberTop, SeparatorWidth, NumberHeight};
constexpr PageRectangle YearNumberBounds = {360, NumberTop, 176, NumberHeight};

constexpr int HourArrowLeft = 200;
constexpr int MinuteArrowLeft = 336;
constexpr PageRectangle HourNumberBounds = {192, NumberTop, 88, NumberHeight};
constexpr PageRectangle ColonBounds = {280, NumberTop, SeparatorWidth, NumberHeight};
constexpr PageRectangle MinuteNumberBounds = {328, NumberTop, 88, NumberHeight};

}

CalendarPages::CalendarPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings, const Settings::ConsoleClock& consoleClock)
    : navigator(pageNavigator)
    , settings(systemSettings)
    , clock(consoleClock)
{
}

bool CalendarPages::Owns(PageId page) const
{
    return page == PageId::CalendarIndex || page == PageId::DateSet || page == PageId::TimeSet;
}

// The pages start with the date and the time of the console. A date after the last year of the console is shown as its last day.
void CalendarPages::EnterPage(PageId page)
{
    Settings::CalendarTime now = clock.Now();

    if (page == PageId::DateSet)
    {
        bool isAfterTheLastYear = now.year > Settings::LastYearOfTheConsole;
        year = std::min(now.year, Settings::LastYearOfTheConsole);
        month = isAfterTheLastYear ? MonthsInYear : now.month;
        day = isAfterTheLastYear ? Settings::ConsoleClock::DaysInMonth(year, month) : now.day;
    }
    else if (page == PageId::TimeSet)
    {
        hour = now.hour;
        minute = now.minute;
    }
}

void CalendarPages::BuildPage(PageId page, Page& content)
{
    if (page == PageId::CalendarIndex)
        BuildCalendarIndex(content);
    else if (page == PageId::DateSet)
        BuildDateSet(content);
    else
        BuildTimeSet(content);
}

void CalendarPages::BuildCalendarIndex(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, u"Calendar");

    builder.AddChoice(0, 2, u"Date", false, [this] { navigator.GoTo(PageId::DateSet); }).sound = PageSound::Decide;
    builder.AddChoice(1, 2, u"Time", false, [this] { navigator.GoTo(PageId::TimeSet); }).sound = PageSound::Decide;
    builder.AddBackButton([this] { navigator.GoTo(PageId::FirstIndex); });
}

void CalendarPages::BuildDateSet(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Date");

    builder.AddStepArrow(ButtonLook::ArrowUp, MonthArrowLeft, UpArrowTop, "MonthUp", [this] { ChangeMonth(1); });
    builder.AddStepArrow(ButtonLook::ArrowDown, MonthArrowLeft, DownArrowTop, "MonthDown", [this] { ChangeMonth(-1); });
    builder.AddStepArrow(ButtonLook::ArrowUp, DayArrowLeft, UpArrowTop, "DayUp", [this] { ChangeDay(1); });
    builder.AddStepArrow(ButtonLook::ArrowDown, DayArrowLeft, DownArrowTop, "DayDown", [this] { ChangeDay(-1); });
    builder.AddStepArrow(ButtonLook::ArrowUp, YearArrowLeft, UpArrowTop, "YearUp", [this] { ChangeYear(1); });
    builder.AddStepArrow(ButtonLook::ArrowDown, YearArrowLeft, DownArrowTop, "YearDown", [this] { ChangeYear(-1); });

    builder.AddText(MonthNumberBounds, TextRole::BigNumber, {NumberText(month, MinimumDigitsOfDateAndTime)});
    builder.AddText(FirstSlashBounds, TextRole::BigNumber, {u"/"});
    builder.AddText(DayNumberBounds, TextRole::BigNumber, {NumberText(day, MinimumDigitsOfDateAndTime)});
    builder.AddText(SecondSlashBounds, TextRole::BigNumber, {u"/"});
    builder.AddText(YearNumberBounds, TextRole::BigNumber, {NumberText(year, DigitsOfYear)});

    builder.AddBackButton([this] { navigator.GoTo(PageId::CalendarIndex); });
    builder.AddBottomRight(u"Confirm", [this] { SaveDate(); });
}

void CalendarPages::BuildTimeSet(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Time");

    builder.AddStepArrow(ButtonLook::ArrowUp, HourArrowLeft, UpArrowTop, "HourUp", [this] { ChangeHour(1); });
    builder.AddStepArrow(ButtonLook::ArrowDown, HourArrowLeft, DownArrowTop, "HourDown", [this] { ChangeHour(-1); });
    builder.AddStepArrow(ButtonLook::ArrowUp, MinuteArrowLeft, UpArrowTop, "MinuteUp", [this] { ChangeMinute(1); });
    builder.AddStepArrow(ButtonLook::ArrowDown, MinuteArrowLeft, DownArrowTop, "MinuteDown", [this] { ChangeMinute(-1); });

    builder.AddText(HourNumberBounds, TextRole::BigNumber, {NumberText(hour, MinimumDigitsOfDateAndTime)});
    builder.AddText(ColonBounds, TextRole::BigNumber, {u":"});
    builder.AddText(MinuteNumberBounds, TextRole::BigNumber, {NumberText(minute, MinimumDigitsOfDateAndTime)});

    builder.AddBackButton([this] { navigator.GoTo(PageId::CalendarIndex); });
    builder.AddBottomRight(u"Confirm", [this] { SaveTime(); });
}

// The year goes round from the last year of the console to the first.
void CalendarPages::ChangeYear(int change)
{
    int yearCount = Settings::LastYearOfTheConsole - Settings::FirstYearOfTheConsole + 1;
    year = Settings::FirstYearOfTheConsole + (year - Settings::FirstYearOfTheConsole + change + yearCount) % yearCount;
    KeepDayInMonth();
    navigator.Refresh();
}

void CalendarPages::ChangeMonth(int change)
{
    month = (month - 1 + change + MonthsInYear) % MonthsInYear + 1;
    KeepDayInMonth();
    navigator.Refresh();
}

// The day goes round from the last day of the month to the first.
void CalendarPages::ChangeDay(int change)
{
    int dayCount = Settings::ConsoleClock::DaysInMonth(year, month);
    day = (day - 1 + change + dayCount) % dayCount + 1;
    navigator.Refresh();
}

void CalendarPages::ChangeHour(int change)
{
    hour = (hour + change + HoursInDay) % HoursInDay;
    navigator.Refresh();
}

void CalendarPages::ChangeMinute(int change)
{
    minute = (minute + change + MinutesInHour) % MinutesInHour;
    navigator.Refresh();
}

void CalendarPages::KeepDayInMonth()
{
    day = std::min(day, Settings::ConsoleClock::DaysInMonth(year, month));
}

// The date changes and the time of day stays.
void CalendarPages::SaveDate()
{
    Settings::CalendarTime time = clock.Now();
    time.year = year;
    time.month = month;
    time.day = day;

    settings.SetClockBias(clock.BiasForTime(time));
    settings.Save();
    navigator.GoTo(PageId::CalendarIndex);
}

// The time of day changes to the minute, and the date stays.
void CalendarPages::SaveTime()
{
    Settings::CalendarTime time = clock.Now();
    time.hour = hour;
    time.minute = minute;
    time.second = 0;

    settings.SetClockBias(clock.BiasForTime(time));
    settings.Save();
    navigator.GoTo(PageId::CalendarIndex);
}

}
