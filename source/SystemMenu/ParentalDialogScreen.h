#pragma once

#include "SystemMenu/ButtonGroup.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/ParentalAnswer.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <string>

namespace SystemMenu
{

// The window that asks for the PIN of the parental controls before something that they restrict (screen 0x1b, prntDlg.ash).
// It has three buttons: Quit, OK, and the field of the PIN. The field opens the entry of the PIN, OK checks it. A wrong PIN shows a
// message and the user may try again, three times. The screen ends with a ParentalAnswer, which it puts into the menu services.
// The argument says what the message of a right PIN tells: 0 that the restrictions are removed until the user is back at the Wii Menu,
// anything else until the user has finished what the restriction stood in the way of.
class ParentalDialogScreen : public Screen
{
public:
    ParentalDialogScreen(MenuServices& menuServices, int messageChoice);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

protected:
    bool RunMain() override;
    void OnRunEnded() override;
    bool RunClosing() override;

private:
    enum class Phase
    {
        WaitingForButton,
        TypingPin,
        WaitingForDialog,

        // The Quit button was pressed and its animation plays.
        Quitting,

        Finished,
    };

    void ReactToButton(const ButtonEvent& event);
    void Press(std::size_t buttonIndex, int playerIndex);
    void CheckPin();
    void ReadTypedPin();
    void ReadAnswerOfDialog();
    bool IsPinRight() const;

    MenuServices& services;
    int messageChoice;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<ButtonGroup> buttons;

    Phase phase = Phase::WaitingForButton;
    ParentalAnswer answer = ParentalAnswer::None;
    bool hasPin = false;
    std::string pin;
    int wrongPinCount = 0;
};

}
