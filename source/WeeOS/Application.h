#pragma once

#include "Boot/ConsoleStartup.h"
#include "Graphics/FramePacer.h"
#include "Graphics/VulkanGraphicsDevice.h"
#include "Input/RealControllers.h"
#include "Platform/FileLocations.h"
#include "Platform/InputDevices.h"
#include "Platform/PreciseTimer.h"
#include "Platform/RegionalSettings.h"
#include "Platform/Window.h"
#include "Settings/ConsoleClock.h"
#include "Settings/PcSettings.h"
#include "SystemMenu/MenuSystem.h"
#include "WeeOS/PcMicrophone.h"
#include "WeeOS/ConsoleImporter.h"
#include "WeeOS/ProgramSound.h"

#include <filesystem>
#include <memory>
#include <optional>

namespace Channels
{
class ChannelSystem;
}

namespace WeeOS
{

// The file picker of Windows, for nand.bin of a BootMii backup.
std::optional<std::filesystem::path> PickNandWithWindows();

// What the program takes from the PC besides the folders of the console. WeeOS.exe uses the clock and the regional settings of
// Windows, the file WeeOS.settings.txt and the Wii files beside the program, the file picker and the connection of Windows, and shows
// its window; a test gives its own, and its window is never shown (docs/HelperRules.md).
struct HostEnvironment
{
    Settings::PcClock clock = Settings::ConsoleClock::ReadWindowsClock;
    Platform::RegionalSettings regionalSettings = Platform::ReadRegionalSettings();
    std::filesystem::path pcSettingsFile = Platform::ExecutableFolder() / "WeeOS.settings.txt";

    // The Wii files from which the first start makes a new console (tools/PrepareWiiFiles).
    std::filesystem::path wiiFilesFolder = Platform::ExecutableFolder() / "WiiFiles";

    // Who picks the NAND of a Wii to import.
    NandPicker pickNand = PickNandWithWindows;

    // Whether the PC is connected: the console is online through it (docs/DECISIONS.md, Online).
    Ios::PcConnectionCheck isPcConnected = Ios::IsWindowsConnected;
    Platform::WindowVisibility windowVisibility = Platform::WindowVisibility::Hidden;

    // The controllers of the PC: Wii Remotes, Joy-Cons and gamepads, as Windows sees them. A test gives devices of its own.
    std::shared_ptr<Platform::InputDeviceAccess> inputDevices = std::make_shared<Platform::WindowsInputDevices>();
};

// WeeOS.exe: the console starts, a window opens and the System Menu runs in it. A NAND folder without a console gets a new console
// first, made from the Wii files (NewConsole.h), and its first start offers the import of the person's own Wii. When the menu starts a
// channel, IOS restarts with the IOS of the channel and the channel's program runs in the window, until it asks for the Wii Menu
// (Boot::ConsoleStartup::LaunchTitle).
// The mouse over the window is the hand of remote 1: the left button is A, the right one B, the wheel Plus and Minus; the keys of the
// PC press the other buttons (docs/DECISIONS.md, PC controls). Real controllers take the remotes 1 to 4 as they connect; the first one
// shares remote 1 with the mouse and the keys. Closing the window asks "Turn off WeeOS?" and Yes is the POWER button.
// The window starts where the settings of the PC say, in fullscreen unless they say otherwise, and they remember where the user puts it.
class Application
{
public:
    // Makes the folder of the SD card and, when there is none, a new console; then powers the console on.
    // Throws when the console cannot be made or cannot start, or the window cannot open.
    // A silent application gives its sound output only silence.
    Application(const Ios::IosFolders& folders, bool isSilent, HostEnvironment hostEnvironment = HostEnvironment());
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Shows pictures until the console is powered off.
    void Run();

    // One picture: handles the events of the window, runs the steps of the menu that are due and shows the newest state. It waits for
    // the display of a shown window, or one step of the console for a hidden one.
    void ShowNextPicture();

    // Exactly one step of the menu or the channel, whatever the clock says, with the events of the window before it and what the program
    // asked for after it. A test that counts steps calls this, and ShowPicture whenever it wants the window to show the newest state.
    void RunOneStep();
    void ShowPicture();

    bool IsPoweredOff() const;

    // The buttons on the front of the console, which the PC has no keys for. A test presses them here.
    void PressPowerButton();
    void PressResetButton();

    Platform::Window& GetWindow();
    Graphics::VulkanGraphicsDevice& GetDevice();
    // The menu. Throws std::logic_error while a channel runs.
    SystemMenu::MenuSystem& GetMenu();

    // The channel that runs. Nothing while the menu runs.
    Channels::ChannelSystem* GetChannel();
    ProgramSound& GetSound();

    // The virtual remote 1 of the session that runs, which the mouse of the window moves. A test moves it too.
    Input::VirtualRemote& GetVirtualRemote();
    // The title whose program runs: the System Menu, or a channel that the menu started.
    Ios::TitleId RunningTitle() const;

    // The services of the IOS that runs. A test takes the SD card out of the slot here.
    Ios::IosServices& GetServices();
    // The Wii Speak accessory is the default microphone of Windows, when the PC has one. A test gives its own source instead, from the
    // session that runs on; the source must outlive the application.
    void UseMicrophoneSource(Ios::MicrophoneSource& source);
    const PcMicrophone& GetPcMicrophone() const;

private:
    // Everything that depends on the IOS that runs: a reset starts IOS again and makes a new session.
    class Session;

    void PrepareConsoleFolders();
    void ImportTheOwnWii();
    void StartSession();
    void AttachMicrophone();
    void StartImportedConsole();
    void StopOnTheKeyOfKoreanConsoles();
    void PlaceWindow();
    void HandleTheWindow();
    void AskBeforeClosing();
    void RememberWindowPlace();
    void WaitForTheNextStepOfAHiddenWindow();
    void RunDueSteps();
    void RunStep();
    void CarryOutRequestsOfTheMenu();
    void ResetConsole();
    void LaunchTitle(Ios::TitleId titleId);
    void RecordReturnToTheMenu();
    void ForgetReportedProblems();
    void ReportProblems();

    Ios::IosFolders folders;
    HostEnvironment host;

    // Before the console, so that the microphone service of IOS can still stop a capture session while the console goes.
    PcMicrophone pcMicrophone;
    Ios::MicrophoneSource* givenMicrophone = nullptr;

    ConsoleImporter importer;
    Boot::ConsoleStartup console;
    Settings::PcSettingsStore pcSettings;
    Platform::Window window;
    Graphics::VulkanGraphicsDevice device;
    ProgramSound sound;
    Input::RealControllers controllers;
    double controllerSeconds = 0;
    std::unique_ptr<Session> session;
    Platform::PreciseTimer timer;
    Graphics::FramePacer pacer;

    bool isPoweredOff = false;
    bool hasReportedFatalError = false;
    std::optional<SystemMenu::ScreenId> reportedUnbuiltScreen;
};

}
