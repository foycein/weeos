#include "WeeOS/Application.h"

#include "Platform/FileLocations.h"

#include <exception>
#include <filesystem>
#include <iostream>

// WeeOS.exe [nand folder [identity folder]]
// The NAND folder is the one NandTool import makes. The identity folder holds the keys.txt, device.cert and seeprom.bin of the console.
// Without arguments the folders are data/nand and data/identity beside the program.

namespace
{

Ios::IosFolders ChooseFolders(int argumentCount, char** arguments)
{
    std::filesystem::path dataFolder = Platform::ExecutableFolder() / "data";

    Ios::IosFolders folders;
    folders.nandFolder = argumentCount > 1 ? std::filesystem::path(arguments[1]) : dataFolder / "nand";
    folders.identityFolder = argumentCount > 2 ? std::filesystem::path(arguments[2]) : dataFolder / "identity";
    return folders;
}

}

int main(int argumentCount, char** arguments)
{
    try
    {
        WeeOS::Application application(ChooseFolders(argumentCount, arguments));
        application.Run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "WeeOS cannot run: " << error.what() << "\n";
        return 1;
    }
}
