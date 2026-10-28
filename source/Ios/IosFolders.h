#pragma once

#include <filesystem>

namespace Ios
{

// The folders on the PC where the console keeps its data. Each service that needs a folder adds it here.
struct IosFolders
{
    // The NAND: the same names and the same layout as on the console.
    std::filesystem::path nandFolder;

    // The identity of the console: the files keys.txt, device.cert and seeprom.bin of a console dump. They are never committed or shipped with the source.
    std::filesystem::path identityFolder;
};

}
