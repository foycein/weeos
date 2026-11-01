#pragma once

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/TitleContents.h"

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
    // The device, the contents and the remote service must outlive the menu.
    MenuSystem(Graphics::GraphicsDevice& device, TitleContents& titleContents, Ios::RemoteService& remoteService,
        Platform::ScreenShape screenShape);

    // One step of the console's picture, 1001/60000 of a second: reads the remotes, updates the screens, the hands and the fader.
    void Update();

    // Starts a new picture of the screen and draws it. The program shows it with Present.
    void Draw();

    // Draws the picture into the picture that is open, for example an offscreen picture.
    void DrawScene();

    MenuServices& Services();

private:
    Display display;
    MenuFiles files;
    Text::MessageCatalog messages;
    MenuClock clock;
    Fader fader;
    InputManager input;
    CursorManager cursors;
    HomeButton homeButton;
    ScreenManager screens;
    MenuServices services;
};

}
