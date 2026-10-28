#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// "Language": the languages that the menu speaks. WeeOS speaks English only for now (docs/DECISIONS.md), so English is the one choice.
class LanguagePage : public PageGroup
{
public:
    // The navigator and the settings must outlive the page.
    LanguagePage(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings);

    bool Owns(PageId page) const override;
    void BuildPage(PageId page, Page& content) override;

private:
    PageNavigator& navigator;
    Settings::SystemSettings& settings;
};

}
