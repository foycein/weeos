#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// The three pages that lead to all the others: "Wii System Settings 1", "2" and "3".
// The second page has "PC Settings" where the original has WiiConnect24, which WeeOS does not have (docs/DECISIONS.md).
// Internet, Country, Wii System Update and Format ask for the PIN of the parental controls first when they are on, as the original does.
class IndexPages : public PageGroup
{
public:
    // The navigator and the settings must outlive the pages.
    IndexPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings);

    bool Owns(PageId page) const override;
    void BuildPage(PageId page, Page& content) override;

private:
    void BuildFirstIndex(Page& content);
    void BuildSecondIndex(Page& content);
    void BuildThirdIndex(Page& content);
    void OpenParentalPages();
    void OpenRestrictedPage(PageId page);

    PageNavigator& navigator;
    Settings::SystemSettings& settings;
};

}
