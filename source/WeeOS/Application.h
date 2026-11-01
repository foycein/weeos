#pragma once

#include "Boot/ConsoleStartup.h"
#include "Graphics/FramePacer.h"
#include "Graphics/VulkanGraphicsDevice.h"
#include "Platform/FileLocations.h"
#include "Platform/HostKeys.h"
#include "Platform/PreciseTimer.h"
#include "Platform/RegionalSettings.h"
#include "Platform/Window.h"
#include "Settings/ConsoleClock.h"
#include "SystemMenu/MenuSystem.h"
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

// What the program takes from the PC besides the folders of the console. WeeOS.exe uses the clock and the regional settings of Windows,
// the file WeeOS.settings.txt and the Wii files beside the program, and the file picker of Windows; a test gives its own.
struct HostEnvironment
{
    Settings::PcClock clock = Settings::ConsoleClock::ReadWindowsClock;
    Platform::RegionalSettings regionalSettings = Platform::ReadRegionalSettings();
    std::filesystem::path pcSettingsFile = Platform::ExecutableFolder() / "WeeOS.settings.txt";

    // The Wii files from which the first start makes a new console (tools/PrepareWiiFiles).
    std::filesystem::path wiiFilesFolder = Platform::ExecutableFolder() / "WiiFiles";

    // Who picks the NAND of a Wii to import.
    NandPicker pickNand = PickNandWithWindows;
};

// WeeOS.exe: the console starts, a window opens and the System Menu runs in it. A NAND folder without a console gets a new console
// first, made from the Wii files (NewConsole.h), and its first start offers the import of the person's own Wii. When the menu starts a channel, IOS restarts with the IOS
// of the channel and the channel's program runs in the window, until it asks for the Wii Menu (Boot::ConsoleStartup::LaunchTitle).
// The mouse over the window is the hand of remote 1, the left mouse button is A and the right one is B.
// Ctrl+Q is the POWER button (and so is closing the window), Ctrl+R is the RESET button and Ctrl+S puts the SD card in or takes it out.
class Application : private Platform::HostCommandListener
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

    // One picture: handles the events of the window, runs the steps of the menu that are due and shows the newest state.
    void ShowNextPicture();

    // Exactly one step of the menu or the channel, whatever the clock says, with the events of the window before it and what the program
    // asked for after it. A test that counts steps calls this, and ShowPicture whenever it wants the window to show the newest state.
    void RunOneStep();
    void ShowPicture();

    bool IsPoweredOff() const;

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

private:
    // Everything that depends on the IOS that runs: a reset starts IOS again and makes a new session.
    class Session;

    void OnHostCommand(Platform::HostCommand command) override;

    void PrepareConsoleFolders();
    void StartSession();
    void StartImportedConsole();
    void StopOnTheKeyOfKoreanConsoles();
    void ReportCloseOfTheWindowAsPowerButton();
    void RunDueSteps();
    void CarryOutRequestsOfTheMenu();
    void ResetConsole();
    void LaunchTitle(Ios::TitleId titleId);
    void RecordReturnToTheMenu();
    void ForgetReportedProblems();
    void ToggleSdCard();
    void ReportProblems();

    Ios::IosFolders folders;
    HostEnvironment host;
    ConsoleImporter importer;
    Boot::ConsoleStartup console;
    Platform::Window window;
    Graphics::VulkanGraphicsDevice device;
    ProgramSound sound;
    std::unique_ptr<Session> session;
    Platform::PreciseTimer timer;
    Graphics::FramePacer pacer;

    bool isPoweredOff = false;
    bool hasReportedClose = false;
    bool hasReportedFatalError = false;
    std::optional<SystemMenu::ScreenId> reportedUnbuiltScreen;
};

}
