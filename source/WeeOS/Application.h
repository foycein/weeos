#pragma once

#include "WeeOS/Console.h"
#include "WeeOS/WiiSettingsServices.h"

#include "Graphics/FramePacer.h"
#include "Graphics/VulkanGraphicsDevice.h"
#include "Input/VirtualRemote.h"
#include "Ios/Keyboard/PcKeyboard.h"
#include "Platform/PreciseTimer.h"
#include "Platform/Window.h"
#include "SystemMenu/MenuSystem.h"
#include "SystemMenu/OwnTitleContents.h"

#include <optional>

namespace WeeOS
{

// WeeOS.exe: the console starts, a window opens and the System Menu runs in it.
// The mouse over the window is the hand of remote 1, the left mouse button is A and the right one is B.
class Application
{
public:
    // Throws when the console cannot start or the window cannot open.
    explicit Application(const Ios::IosFolders& folders);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Shows pictures until the user closes the window.
    void Run();

    // One picture: handles the events of the window, runs the steps of the menu that are due and shows the newest state.
    void ShowNextPicture();

    bool IsCloseRequested() const;

    Platform::Window& GetWindow();
    Graphics::VulkanGraphicsDevice& GetDevice();
    SystemMenu::MenuSystem& GetMenu();

private:
    Input::VirtualRemote& GetVirtualRemote();
    void RunDueSteps();
    void ReportUnbuiltScreen();

    Console console;
    Platform::Window window;
    Ios::PcKeyboard keyboard;
    Graphics::VulkanGraphicsDevice device;
    SystemMenu::OwnTitleContents titleContents;
    WiiSettingsServices wiiSettings;
    SystemMenu::MenuSystem menu;
    Platform::PreciseTimer timer;
    Graphics::FramePacer pacer;
    std::optional<SystemMenu::ScreenId> reportedUnbuiltScreen;
};

}
