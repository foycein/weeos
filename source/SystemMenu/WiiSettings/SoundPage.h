#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// "Sound": mono, stereo or surround. The sound changes at once, so that the user hears the choice; Back puts the old mode back.
class SoundPage : public PageGroup
{
public:
    // The navigator and the settings must outlive the page.
    SoundPage(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void ChooseSoundMode(Settings::SoundMode soundMode);

    PageNavigator& navigator;
    Settings::SystemSettings& settings;
    Settings::SoundMode soundModeAtEntry = Settings::SoundMode::Stereo;
};

}
