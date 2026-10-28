#pragma once

#include "SystemMenu/MenuSettings.h"
#include "SystemMenu/NandFiles.h"
#include "SystemMenu/ScreenId.h"

namespace SystemMenu
{

struct FirstScreen
{
    ScreenId screen;
    int argument;
};

// How the menu was started: from the power button, from a title that ended, or with a request for a page of the settings.
enum class BootType
{
    ColdStart,
    ReturnFromTitle,
    ReturnToSettings,
    LaunchWithArguments,
};

// What decides which screen shows first (docs/audit/OpenPoints/SystemMenu.md SYS-16).
struct StartFacts
{
    // The first-time setup of the console is done.
    bool isConsoleSetUp = true;

    // The update type of the console. It tells how far a setup that was not finished had come.
    int updateType = 0;

    // A title that has a time limit ran out while it ran, and the menu starts after it.
    bool hasExpiredTitle = false;

    BootType bootType = BootType::ColdStart;

    // Launch with arguments only: what the title that started the menu wants (1 for a page of the settings, 2 for Wii Options).
    int launchCode = 0;
};

// The arguments of the start-up router: where it takes the user.
constexpr int RouterToWiiOptions = 0;
constexpr int RouterToSettingsCountryPage = 1;

// The screen that shows first, as the original chooses it (0x813DA8FC). A cold start with everything set up shows Health and Safety.
FirstScreen ChooseFirstScreen(const StartFacts& facts);

// Reads the facts from the files of the NAND: the settings, the state file of the System Menu, the boot information and the record of
// an expired title. The record is deleted when the menu does not start after a title, as the original does.
StartFacts ReadStartFacts(NandFiles& nand, MenuSettings& settings);

}
