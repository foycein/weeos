#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/NandFiles.h"
#include "SystemMenu/ScreenManager.h"

#include "Ios/Power/PowerService.h"
#include "Ios/Title/TitleId.h"

#include <optional>

namespace SystemMenu
{

// The POWER and RESET buttons of the console, as the menu handles them (0x81356904, docs/audit/OpenPoints/SystemMenu.md SYS-07).
// After a press the menu waits until every screen can stop, tells the screens to stop and fades the picture to black with a fader
// of its own. When the picture is black and every screen has stopped, it writes how it left into state.dat and asks for:
//  - POWER: the console switches off.
//  - RESET before the user has reached the Wii Menu: the console starts again from the beginning.
//  - RESET later: the System Menu starts again as a title and shows "Back to Wii Menu".
// While the error screen is shown, RESET does nothing and POWER does not wait for the screens.
class FrontPanel
{
public:
    // Everything given must stay alive as long as this object.
    FrontPanel(Display& menuDisplay, Ios::PowerService& powerService, ScreenManager& screenManager, NandFiles& nandFiles);

    // A screen presses a button in place of the user: the HOME menu and Wii Settings ask for a reset like this.
    // A press while another one is carried out has no effect.
    void PressReset();
    void PressPower();

    // True from a press on.
    bool IsWorking() const;

    // From now on RESET leads back to the Wii Menu (see FirstScreen.h, IsMenuReachedAtStart).
    void NoteMenuReached();

    // Once in every step.
    void Update(bool isErrorScreenShown);

    // Draws the black in front of everything.
    void Draw();

    // The title that the console should start now: the System Menu, after RESET. Nothing before that, and nothing after it was taken.
    std::optional<Ios::TitleId> TakeTitleToLaunch();

private:
    enum class Phase
    {
        Idle,
        WaitingForScreens,
        FadingOut,
        Done,
    };

    void TakeButtonOfThePlatform(bool isErrorScreenShown);
    void Press(Ios::PowerEvent button);
    void WaitForScreens(bool isErrorScreenShown);
    void WaitForTheBlack(bool isErrorScreenShown);
    void CarryOut();

    Ios::PowerService& power;
    ScreenManager& screens;
    NandFiles& nand;
    Fader fader;

    Phase phase = Phase::Idle;
    Ios::PowerEvent pressedButton = Ios::PowerEvent::PowerButtonPressed;
    bool isMenuReached = false;
    std::optional<Ios::TitleId> titleToLaunch;
};

}
