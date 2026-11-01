#include "WeeOS/Application.h"

#include "Platform/FileLocations.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

// WeeOS.exe [--silent] [--hidden] [nand folder [identity folder]]
// The NAND folder is the one NandTool import makes. The identity folder holds the keys.txt, device.cert and seeprom.bin of the console.
// Without folders they are data/nand and data/identity beside the program.
// The SD card is the folder sd beside the NAND folder, always in the slot.
// --silent opens the sound output with nothing but silence, and --hidden draws into a window that is never shown, for the runs of
// development (docs/HelperRules.md). Without them WeeOS plays its sound and shows its window.

namespace
{

constexpr std::string_view SilentOption = "--silent";
constexpr std::string_view HiddenOption = "--hidden";

Ios::IosFolders ChooseFolders(const std::vector<std::string_view>& folderArguments)
{
    std::filesystem::path dataFolder = Platform::ExecutableFolder() / "data";

    Ios::IosFolders folders;
    folders.nandFolder = folderArguments.size() > 0 ? std::filesystem::path(folderArguments[0]) : dataFolder / "nand";
    folders.identityFolder = folderArguments.size() > 1 ? std::filesystem::path(folderArguments[1]) : dataFolder / "identity";
    folders.sdCardFolder = folders.nandFolder.parent_path() / "sd";
    return folders;
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
        std::cerr << "WeeOS cannot run: " << error.what() << "\n";
        return 1;
    }
}
