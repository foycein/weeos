#pragma once

#include "Boot/ConsoleStartup.h"
#include "Graphics/FramePacer.h"
#include "Graphics/VulkanGraphicsDevice.h"
#include "Platform/HostKeys.h"
#include "Platform/PreciseTimer.h"
#include "Platform/Window.h"
#include "SystemMenu/MenuSystem.h"

#include <memory>
#include <optional>

namespace WeeOS
{

// WeeOS.exe: the console starts, a window opens and the System Menu runs in it. When the menu starts a channel, IOS restarts with the IOS
// of the channel and the channel's program runs in the window, until it asks for the Wii Menu (Boot::ConsoleStartup::LaunchTitle).
// The mouse over the window is the hand of remote 1, the left mouse button is A and the right one is B.
// Ctrl+Q is the POWER button (and so is closing the window), Ctrl+R is the RESET button and Ctrl+S puts the SD card in or takes it out.
class Application : private Platform::HostCommandListener
{
public:
    // Powers the console on. Throws when the console cannot start or the window cannot open.
    explicit Application(const Ios::IosFolders& folders);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Shows pictures until the console is powered off.
    void Run();

    // One picture: handles the events of the window, runs the steps of the menu that are due and shows the newest state.
    void ShowNextPicture();

    bool IsPoweredOff() const;

    Platform::Window& GetWindow();
    Graphics::VulkanGraphicsDevice& GetDevice();
    // The menu. Throws std::logic_error while a channel runs.
    SystemMenu::MenuSystem& GetMenu();

    // The virtual remote 1 of the session that runs, which the mouse of the window moves. A test moves it too.
    Input::VirtualRemote& GetVirtualRemote();
    // The title whose program runs: the System Menu, or a channel that the menu started.
    Ios::TitleId RunningTitle() const;

private:
    // Everything that depends on the IOS that runs: a reset starts IOS again and makes a new session.
    class Session;

    void OnHostCommand(Platform::HostCommand command) override;

    void StartSession();
    void StopOnTheKeyOfKoreanConsoles();
    void ReportCloseOfTheWindowAsPowerButton();
    void RunDueSteps();
    void CarryOutRequestsOfTheMenu();
    void ResetConsole();
    void LaunchTitle(Ios::TitleId titleId);
    void ForgetReportedProblems();
    void ToggleSdCard();
    void ReportProblems();

    Ios::IosFolders folders;
    Boot::ConsoleStartup console;
    Platform::Window window;
    Graphics::VulkanGraphicsDevice device;
    std::unique_ptr<Session> session;
    Platform::PreciseTimer timer;
    Graphics::FramePacer pacer;

    bool isPoweredOff = false;
    bool hasReportedClose = false;
    bool hasReportedFatalError = false;
    std::optional<SystemMenu::ScreenId> reportedUnbuiltScreen;
};

}
