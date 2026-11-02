#include "WeeOS/Application.h"

#include "Platform/ErrorMessage.h"
#include "Platform/FileLocations.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// WeeOS.exe [--silent] [--hidden] [--import nand.bin] [nand folder [identity folder]]
// WeeOS is one portable folder. Beside WeeOS.exe are:
//   NAND                 the NAND of the console (made at the first start, or imported from a Wii)
//   Identity             who the console is: keys.txt, device.cert, seeprom.bin (and authority.cert for a console that WeeOS made)
//   SDCard               the SD card, always in its slot; files copied into it or out of it show up at once
//   WeeOS.settings.txt   the settings of the PC: window, sound, keys
//   WiiFiles             the Wii files that a new console is made from (only in development; without them the first start imports
//                        the person's own Wii)
//   Browser              the cache and the cookies of the browser engine of the Internet Channel
// Folders on the command line are for tests and development; the SD card is then the folder SDCard beside the NAND folder.
// --silent opens the sound output with nothing but silence, and --hidden draws into a window that is never shown, for the runs of
// development (docs/HelperRules.md). Without them WeeOS plays its sound and shows its window. --import gives the nand.bin of the
// first start's import instead of the file picker.
// When WeeOS cannot start it says why in a message box, except with --silent, where nobody may be asked anything: there the reason
// only goes to the error output.

namespace
{

constexpr std::string_view SilentOption = "--silent";
constexpr std::string_view HiddenOption = "--hidden";
constexpr std::string_view ImportOption = "--import";
constexpr std::string_view NandFolderName = "NAND";
constexpr std::string_view IdentityFolderName = "Identity";
constexpr std::string_view SdCardFolderName = "SDCard";

constexpr std::string_view CannotStartTitle = "WeeOS cannot start";
constexpr std::string_view WhatToDo =
    "What you can do:\n"
    "- Update the driver of your graphics card. WeeOS needs Vulkan 1.3.\n"
    "- Check that the WeeOS folder is complete: WeeOS.exe, and the three .spv files beside it.\n"
    "- Unpack or install WeeOS again if something is missing. Your NAND, Identity and SDCard folders stay as they are.";

Ios::IosFolders ChooseFolders(const std::vector<std::string_view>& folderArguments)
{
    std::filesystem::path programFolder = Platform::ExecutableFolder();

    Ios::IosFolders folders;
    folders.nandFolder = programFolder / NandFolderName;
    folders.identityFolder = programFolder / IdentityFolderName;
    if (folderArguments.size() > 0)
        folders.nandFolder = folderArguments[0];
    if (folderArguments.size() > 1)
        folders.identityFolder = folderArguments[1];

    folders.sdCardFolder = folders.nandFolder.parent_path() / SdCardFolderName;
    return folders;
}

void ReportWhyWeeOSCannotStart(const std::exception& error, bool isSilent)
{
    std::cerr << "WeeOS cannot run: " << error.what() << "\n";
    if (isSilent)
        return;

    std::string message = std::string(error.what()) + "\n\n" + std::string(WhatToDo);
    Platform::ShowErrorMessage(CannotStartTitle, message);
}

}

int main(int argumentCount, char** arguments)
{
    bool isSilent = false;
    WeeOS::HostEnvironment host;
    host.windowVisibility = Platform::WindowVisibility::Shown;

    std::vector<std::string_view> folderArguments;
    for (int argumentNumber = 1; argumentNumber < argumentCount; ++argumentNumber)
    {
        std::string_view argument = arguments[argumentNumber];
        if (argument == SilentOption)
            isSilent = true;
        else if (argument == HiddenOption)
            host.windowVisibility = Platform::WindowVisibility::Hidden;
        else if (argument == ImportOption && argumentNumber + 1 < argumentCount)
        {
            std::filesystem::path nandImage = arguments[++argumentNumber];
            host.pickNand = [nandImage] { return std::optional<std::filesystem::path>(nandImage); };
        }
        else
            folderArguments.push_back(argument);
    }

    try
    {
        WeeOS::Application application(ChooseFolders(folderArguments), isSilent, host);
        application.Run();
        return 0;
    }
    catch (const std::exception& error)
    {
        ReportWhyWeeOSCannotStart(error, isSilent);
        return 1;
    }
}
