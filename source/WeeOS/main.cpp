#include "WeeOS/Application.h"

#include "Platform/FileLocations.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

// WeeOS.exe [--silent] [nand folder [identity folder]]
// WeeOS is one portable folder. Beside WeeOS.exe are:
//   NAND                 the NAND of the console (made at the first start, or imported from a Wii)
//   Identity             who the console is: keys.txt, device.cert, seeprom.bin (and authority.cert for a console that WeeOS made)
//   SDCard               the SD card, always in its slot; files copied into it or out of it show up at once
//   WeeOS.settings.txt   the settings of the PC: window, sound, keys
//   WiiFiles             the Wii files that a new console is made from
//   Browser              the cache and the cookies of the browser engine of the Internet Channel
// Folders on the command line are for tests and development; the SD card is then the folder SDCard beside the NAND folder.
// --silent opens the sound output with nothing but silence, for the runs of development. Without it WeeOS plays its sound.

namespace
{

constexpr std::string_view SilentOption = "--silent";
constexpr std::string_view NandFolderName = "NAND";
constexpr std::string_view IdentityFolderName = "Identity";
constexpr std::string_view SdCardFolderName = "SDCard";

Ios::IosFolders ChooseFolders(const std::vector<std::string_view>& folderArguments)
{
    std::filesystem::path programFolder = Platform::ExecutableFolder();

    Ios::IosFolders folders;
    folders.nandFolder = folderArguments.size() > 0 ? std::filesystem::path(folderArguments[0]) : programFolder / NandFolderName;
    folders.identityFolder = folderArguments.size() > 1 ? std::filesystem::path(folderArguments[1]) : programFolder / IdentityFolderName;
    folders.sdCardFolder = folders.nandFolder.parent_path() / SdCardFolderName;
    return folders;
}

}

int main(int argumentCount, char** arguments)
{
    bool isSilent = false;
    std::vector<std::string_view> folderArguments;
    for (int argumentNumber = 1; argumentNumber < argumentCount; ++argumentNumber)
    {
        std::string_view argument = arguments[argumentNumber];
        if (argument == SilentOption)
            isSilent = true;
        else
            folderArguments.push_back(argument);
    }

    try
    {
        WeeOS::Application application(ChooseFolders(folderArguments), isSilent);
        application.Run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "WeeOS cannot run: " << error.what() << "\n";
        return 1;
    }
}
