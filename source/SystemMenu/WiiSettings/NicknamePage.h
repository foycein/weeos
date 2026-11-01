#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemSettings.h"

#include <string>

namespace SystemMenu
{

// "Console Nickname": the name of the console, up to ten characters. The user types it with the soft keyboard.
class NicknamePage : public PageGroup
{
public:
    // The navigator and the settings must outlive the page.
    NicknamePage(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void AskForNickname();

    PageNavigator& navigator;
    Settings::SystemSettings& settings;
    std::u16string nickname;
};

}
