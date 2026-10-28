#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"

#include "Ios/Power/PowerService.h"

#include <optional>

namespace SystemMenu
{

// The POWER and RESET buttons of the console, as the menu handles them (the front panel of the original, docs/audit/SystemMenu.md SYS-07).
// A press makes the picture fade to black with a fader of its own, the one that no screen touches. When the picture is black the menu
// asks the console for what the button means: to switch off, or to start again. It keeps working while the error screen is shown.
class FrontPanel
{
public:
    // The display and the power service must stay alive as long as this object.
    FrontPanel(Display& menuDisplay, Ios::PowerService& powerService);

    // Once in every step.
    void Update();

    // Draws the black in front of everything.
    void Draw();

private:
    void StartFading(Ios::PowerEvent button);
    void AskForWhatTheButtonMeans();

    Ios::PowerService& power;
    Fader fader;

    // The button that made the picture fade. The buttons that are pressed after it have no effect.
    std::optional<Ios::PowerEvent> pressedButton;
    bool hasAsked = false;
};

}
