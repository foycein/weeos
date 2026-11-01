#pragma once

namespace Text
{
class MessageCatalog;
}

namespace SystemMenu
{

class ConsoleImport;
class CursorManager;
struct DataManagementServices;
class DialogManager;
class Display;
class Fader;
class FrontPanel;
class HomeButton;
class InputManager;
class MenuClock;
class MenuFiles;
class MenuSettings;
class NandFiles;
class ProgramImage;
class ScreenManager;
class SoftKeyboard;
class SoundRequests;
struct SettingsFlowServices;
struct WiiMenuData;
class TitleLaunchRequest;

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

    // The POWER and RESET buttons. A screen can press them too, and see whether one was pressed.
    FrontPanel& frontPanel;

    // What Wii Settings works with. Nothing when the menu runs without it (the tests of the other screens).
    SettingsFlowServices* wiiSettings;

    // The HOME button: the Wii Menu starts it, a screen that must not be interrupted disables it.
    HomeButton& homeButton;
    WiiMenuData& wiiMenu;

    // How the menu starts a channel: titleLaunch.Request(channelTitleId) once the start window has faded out.
    TitleLaunchRequest& titleLaunch;

    // The titles, the NAND, the SD card and the identity for Data Management and the SD Card Menu. Nothing when the menu runs without IOS.
    DataManagementServices* dataManagement = nullptr;

    // The import of a person's own Wii (ConsoleImport.h). Nothing when the program offers none.
    ConsoleImport* consoleImport = nullptr;
};

}
