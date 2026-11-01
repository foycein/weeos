#pragma once

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoftKeyboard.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TitleContents.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Ios/Remote/RemoteService.h"
#include "Text/MessageCatalog.h"

namespace SystemMenu
{

// The System Menu: it makes the managers that the screens work with and runs one step and one picture at a time.
// This is the System object of the original (iplSystem.cpp, docs/research/SystemMenuStructure.md 2.3 and 2.5).
class MenuSystem
{
public:
    // Reads the resources of the menu from the contents of the System Menu title and starts the first screen.
    // The device, the contents, the remote service and the console of the Wii Menu must outlive the menu.
    MenuSystem(Graphics::GraphicsDevice& device, TitleContents& titleContents, Ios::RemoteService& remoteService,
        Platform::ScreenShape screenShape, MenuConsole& wiiMenuConsole);

    // One step of the console's picture, 1001/60000 of a second: reads the remotes, updates the screens, the hands and the fader.
    void Update();

    // Starts a new picture of the screen and draws it. The program shows it with Present.
    void Draw();

    // Draws the picture into the picture that is open, for example an offscreen picture.
    void DrawScene();

    // The keys of this USB keyboard type into the soft keyboard. The service must stay alive as long as the menu.
    void UsePhysicalKeyboard(Ios::KeyboardService& keyboardService);

    MenuServices& Services();

private:
    Display display;
    MenuFiles files;
    MenuFonts fonts;
    Text::MessageCatalog messages;
    MenuClock clock;
    Fader fader;
    InputManager input;
    CursorManager cursors;
    WiiMenuData wiiMenu;
    ScreenManager screens;
    SoundRequests sounds;
    SoftKeyboard keyboard;
    MenuServices services;
};

}
