#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemMemoryFormatter.h"

namespace SystemMenu
{

// "Format Wii System Memory": three questions that make sure the user knows what is lost, the page that works, and the page that says
// that the console switches off.
class FormatPages : public PageGroup
{
public:
    // The navigator and the formatter must outlive the pages.
    FormatPages(PageNavigator& pageNavigator, Settings::SystemMemoryFormatter& systemMemoryFormatter);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;
    void Step(PageId shownPage) override;

private:
    void BuildWarning(Page& content, PageId nextPage, std::vector<std::u16string> lines);
    void BuildConfirm(Page& content);
    void BuildWorking(Page& content);
    void BuildDone(Page& content);
    void BuildFailed(Page& content);

    PageNavigator& navigator;
    Settings::SystemMemoryFormatter& formatter;

    int stepsWorked = 0;
    bool hasFormatted = false;
};

}
