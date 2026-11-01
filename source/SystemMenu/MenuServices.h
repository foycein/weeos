#pragma once

namespace Text
{
class MessageCatalog;
}

namespace SystemMenu
{

class CursorManager;
class Display;
class Fader;
class InputManager;
class MenuClock;
class MenuFiles;
class ScreenManager;
struct WiiMenuData;

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
    WiiMenuData& wiiMenu;
};

}
