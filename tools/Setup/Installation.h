#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace Setup
{

inline constexpr wchar_t UninstallerFileName[] = L"Uninstall WeeOS.exe";

// weeossetup.exe /silent [/folder <folder>] installs without a window; "Uninstall WeeOS.exe" /silent [/removeconsole] removes WeeOS
// without a window and keeps the console unless it is told otherwise.
inline constexpr wchar_t SilentOption[] = L"/silent";
inline constexpr wchar_t FolderOption[] = L"/folder";
inline constexpr wchar_t RemoveConsoleOption[] = L"/removeconsole";

// COM on the current thread, for the shortcuts and the folder picker.
class ComApartment
{
public:
    ComApartment();
    ~ComApartment();

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
};

// The version of WeeOS that is installed, as the welcome screen shows it.
std::wstring WeeOSVersionText();

// Called while WeeOS is installed, with the share of the work that is done: from 0 to 1.
using ProgressReport = std::function<void(double doneShare)>;

// The folder WeeOS in the programs folder of the person (in the local application data), which the person can write to without
// asking Windows for permission.
std::filesystem::path DefaultInstallFolder();

// A folder that the person chose: WeeOS goes into a folder WeeOS inside it, unless it is already called so. The uninstaller removes
// only what it installed, but a folder of its own keeps WeeOS apart from other files.
std::filesystem::path InstallFolderInside(const std::filesystem::path& chosenFolder);

// Unpacks the portable WeeOS into the folder (without touching a console that is already there), writes the uninstaller beside it and
// makes the shortcuts on the desktop and in the Start menu and the entry in the apps of Windows.
// Throws std::runtime_error with a message for the person when something fails.
void InstallWeeOS(const std::filesystem::path& folder, const ProgressReport& reportProgress);

// Removes the program, its shortcuts and its entry in the apps of Windows from the folder of this uninstaller. The console (the NAND,
// the identity, the SD card folder and the settings of the PC) stays, unless removeConsole is true. The uninstaller itself and the
// folder, when it is empty, go a moment after this program has ended.
void UninstallWeeOS(bool removeConsole);

// The folder of the WeeOS that this uninstaller removes: the folder it lies in.
std::filesystem::path InstalledFolder();

void StartWeeOS(const std::filesystem::path& folder);

}
