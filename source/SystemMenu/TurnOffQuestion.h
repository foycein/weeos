#pragma once

#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/SoundRequests.h"

namespace SystemMenu
{

// The question "Turn off WeeOS?" that comes when the user closes the window (docs/DECISIONS.md, PC controls). It is a dialog window of
// the menu with Yes and No, with a dialog manager of its own, so that it can lie over every screen and every dialog of a screen. The
// menu or the channel stands still while it is open. Yes switches the console off the way the POWER button does.
class TurnOffQuestion
{
public:
    // The display, the files and the sound requests must outlive the question.
    TurnOffQuestion(Display& display, const MenuFiles& files, SoundRequests& sounds);

    // Opens the question. A question that is open already stays as it is.
    void Ask();

    // True from Ask until the window has closed again.
    bool IsOpen() const;

    // Once in every step while the question is open. Gives true once, in the step in which the user chose Yes.
    bool Update(const InputManager& input);

    void Draw();

private:
    DialogManager dialog;
};

}
