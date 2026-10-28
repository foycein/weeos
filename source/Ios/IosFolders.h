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

    // The identity of the console: the files keys.txt, device.cert and seeprom.bin of a console dump. They are never committed or shipped with the source.
    // The address of the wireless network card is read from keys.txt too.
    std::filesystem::path identityFolder;
};

}
