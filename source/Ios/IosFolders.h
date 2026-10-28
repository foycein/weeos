#pragma once

#include <filesystem>

namespace Ios
{

// The folders on the PC where the console keeps its data. Each service that needs a folder adds it here.
struct IosFolders
{
    // The NAND: the same names and the same layout as on the console.
    std::filesystem::path nandFolder;

    // The SD card in the slot. Leave it empty, or name a folder that does not exist, for an empty slot.
    std::filesystem::path sdCardFolder;

    // The private data of the console, never shipped. It holds keys.txt as the console dump has it.
    // Leave it empty to run without: the wireless card then has the address 00:00:00:00:00:00.
    std::filesystem::path identityFolder;
};

}
