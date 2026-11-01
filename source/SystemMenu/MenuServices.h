#pragma once

#include "SystemMenu/ParentalAnswer.h"

namespace Text
{
class MessageCatalog;
}

namespace SystemMenu
{

class CursorManager;
class DialogManager;
class Display;
class Fader;
class FrontPanel;
class InputManager;
class MenuClock;
class MenuFiles;
class MenuSettings;
class NandFiles;
class ProgramImage;
class ScreenManager;
class SoftKeyboard;
class SoundRequests;

// Everything a screen needs from the rest of the menu. The original keeps these objects in global variables,
// the screens of WeeOS get them in one place.
struct MenuServices
{
    Display& display;
    const MenuFiles& files;
    const Text::MessageCatalog& messages;
    const MenuClock& clock;
    Fader& fader;
    InputManager& input;
    CursorManager& cursors;
    ScreenManager& screens;
    SoundRequests& sounds;
    SoftKeyboard& keyboard;
    ProgramImage& program;
    NandFiles& nand;
    MenuSettings& settings;
    DialogManager& dialogs;

    // Where the password dialog of the parental controls leaves its answer.
    ParentalAnswer& parentalAnswer;

    // The POWER and RESET buttons. A screen can press them too, and see whether one was pressed.
    FrontPanel& frontPanel;
};

}
