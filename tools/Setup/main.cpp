#include "Installation.h"
#include "Package.h"
#include "SetupWindow.h"

#include <windows.h>
#include <shellapi.h>

#include <exception>
#include <iostream>
#include <string>
#include <vector>

// weeossetup.exe [/silent] [/folder <folder>]
//   Installs WeeOS: it unpacks the portable WeeOS folder that it carries (Package.h) where the person chooses, by default into
//   Programs\WeeOS of the person's local application data, and makes the shortcuts and the entry in the apps of Windows.
// "Uninstall WeeOS.exe" [/silent] [/removeconsole]
//   The same program without the package, written beside WeeOS.exe by the setup. It removes the program and asks before it removes the
//   console (NAND, identity, SD card); /silent keeps the console unless /removeconsole is given.
// /silent shows no window; a failure is written to the error output and ends with exit code 1.

namespace
{

struct SetupOptions
{
    bool isSilent = false;
    bool removeConsole = false;
    std::filesystem::path folder;
};

std::vector<std::wstring> CommandLineArguments()
{
    int argumentCount = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

    std::vector<std::wstring> argumentList;
    for (int argumentNumber = 1; argumentNumber < argumentCount; ++argumentNumber)
        argumentList.push_back(arguments[argumentNumber]);

    LocalFree(arguments);
    return argumentList;
}

SetupOptions ReadOptions(const std::vector<std::wstring>& arguments)
{
    SetupOptions options;
    for (std::size_t argumentNumber = 0; argumentNumber < arguments.size(); ++argumentNumber)
    {
        const std::wstring& argument = arguments[argumentNumber];
        bool hasNextArgument = argumentNumber + 1 < arguments.size();

        if (argument == Setup::SilentOption)
            options.isSilent = true;
        else if (argument == Setup::RemoveConsoleOption)
            options.removeConsole = true;
        else if (argument == Setup::FolderOption && hasNextArgument)
        {
            options.folder = arguments[argumentNumber + 1];
            argumentNumber += 1;
        }
    }
    return options;
}

int InstallSilently(const std::filesystem::path& folder)
{
    try
    {
        Setup::InstallWeeOS(folder, [](double) {});
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "WeeOS could not be installed: " << error.what() << "\n";
        return 1;
    }
}

int UninstallSilently(bool removeConsole)
{
    try
    {
        Setup::UninstallWeeOS(removeConsole);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "WeeOS could not be removed: " << error.what() << "\n";
        return 1;
    }
}

int Install(const SetupOptions& options)
{
    std::filesystem::path folder = options.folder.empty() ? Setup::DefaultInstallFolder() : options.folder;
    if (options.isSilent)
        return InstallSilently(folder);

    Setup::SetupWindow window(Setup::SetupKind::Install, folder);
    return window.Run();
}

int Uninstall(const SetupOptions& options)
{
    if (options.isSilent)
        return UninstallSilently(options.removeConsole);

    Setup::SetupWindow window(Setup::SetupKind::Uninstall, Setup::InstalledFolder());
    return window.Run();
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Setup::ComApartment comForTheShortcutsAndTheFolderPicker;
    SetupOptions options = ReadOptions(CommandLineArguments());

    try
    {
        if (Setup::ThisProgramCarriesThePackage())
            return Install(options);

        return Uninstall(options);
    }
    catch (const std::exception& error)
    {
        std::cerr << "WeeOS Setup stopped: " << error.what() << "\n";
        return 1;
    }
}
