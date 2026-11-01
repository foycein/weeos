#pragma once

#include "SystemMenu/WiiSettings/PageBuilder.h"
#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/PcSettings.h"
#include "Settings/SystemSettings.h"

#include <functional>

namespace SystemMenu
{

// "PC Settings": the window, the sound and the controls of WeeOS on the PC, in the look of the other pages (docs/DECISIONS.md, Settings).
// Every choice counts at once: the pages store it in the file beside the program and the program makes it true while the user looks.
// The sound mode and the rumble are the console's own settings, so these pages change those (the sound mode on the page SoundSet).
class PcPages : public PageGroup
{
public:
    // The navigator, the settings of the console, the settings of the PC and the program that applies them must outlive the pages.
    // The page "Import a Wii NAND" is offered only when the program can import (canImport).
    PcPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings, Settings::PcSettingsStore& pcSettingsStore,
        Settings::PcSettingsTarget& pcSettingsTarget, bool canImport);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void BuildPcIndex(PageBuilder& builder);
    void BuildWindow(PageBuilder& builder);
    void BuildWindowSize(PageBuilder& builder);
    void BuildSound(PageBuilder& builder);
    void BuildSoundDevice(PageBuilder& builder);
    void BuildControls(PageBuilder& builder);
    void BuildKeys(PageBuilder& builder);
    void BuildMouse(PageBuilder& builder);
    void BuildControllers(PageBuilder& builder);
    void BuildGamepadButtons(PageBuilder& builder);

    // The arrows up and down of a list that has more rows than one page holds.
    void AddListArrows(PageBuilder& builder, int& shownListPage, int listPageCount);

    // Changes the settings of the PC, stores them and makes them true at once.
    void Change(const std::function<void(Settings::PcSettings&)>& change);

    void ToggleRumble();
    void ChangeVolume(int change);
    void ChangeKey(Input::ButtonMask button, int change);
    void ChangeGamepadButton(Input::ButtonMask button);

    PageNavigator& navigator;
    Settings::SystemSettings& settings;
    Settings::PcSettingsStore& pcSettings;
    Settings::PcSettingsTarget& pcTarget;
    bool isImportOffered;

    // The playback devices of the PC, asked for when the user opens their list, and the page of each list that is shown.
    std::vector<std::u16string> audioDevices;
    int shownDevicePage = 0;
    int shownKeyPage = 0;
    int shownGamepadPage = 0;
};

}
