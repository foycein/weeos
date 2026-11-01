#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// "Country": the list of the countries that the console offers, five on a page, with an arrow up and an arrow down to turn the pages.
// The first page has the request "Select your country of residence." above its four countries.
class CountryPages : public PageGroup
{
public:
    // The navigator and the settings must outlive the pages.
    CountryPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void TurnPage(int change);

    PageNavigator& navigator;
    Settings::SystemSettings& settings;

    // The page of the list that is shown, counted from 0, and the code of the country that the user has chosen.
    int shownPage = 0;
    int chosenCountryCode = 0;
};

}
