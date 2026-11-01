#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/ConsoleClock.h"
#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// "Calendar": the page with the two ways to set the clock, the date, and the time of day.
// The pages change the clock bias of the settings, as the original changes its counter bias and not a clock chip.
class CalendarPages : public PageGroup
{
public:
    // The navigator, the settings and the clock must outlive the pages.
    CalendarPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings, const Settings::ConsoleClock& consoleClock);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void BuildCalendarIndex(Page& content);
    void BuildDateSet(Page& content);
    void BuildTimeSet(Page& content);

    void ChangeYear(int change);
    void ChangeMonth(int change);
    void ChangeDay(int change);
    void ChangeHour(int change);
    void ChangeMinute(int change);

    // The day is brought back to the last day of the month when the month is shorter.
    void KeepDayInMonth();

    void SaveDate();
    void SaveTime();

    PageNavigator& navigator;
    Settings::SystemSettings& settings;
    const Settings::ConsoleClock& clock;

    int year = Settings::FirstYearOfTheConsole;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
};

}
