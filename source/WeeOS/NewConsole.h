#pragma once

#include "Ios/IosFolders.h"

#include <filesystem>

namespace WeeOS
{

// True when the NAND folder holds a console: its index (written last by every import and by the file system) is there.
bool HasConsole(const Ios::IosFolders& folders);

// Makes a new console, like a Wii that leaves the factory, from prepared Wii files beside WeeOS.exe:
//  - a console identity of its own, when the identity folder has none (Ios::WriteNewIdentity), with the keys that every Wii shares
//    found in the prepared System Menu and its IOS (Ios::FindSharedKeysInWiiFiles)
//  - every prepared title, installed through the title service, which writes the tickets, the contents, content.map and uid.sys
//  - /sys/cert.sys with Nintendo's certificates, and the files of the System Menu: setting.txt for the region USA and SYSCONF with
//    the settings of a new WeeOS (nickname "WeeOS", English, 16:9, sensor bar below, set up), and the network configuration with
//    WeeOS's connection through the PC
// The NAND folder must be empty or not exist (std::filesystem::filesystem_error otherwise). Throws std::runtime_error, with what went
// wrong, when the console cannot be made.
void MakeNewConsole(const Ios::IosFolders& folders, const std::filesystem::path& wiiFilesFolder);

}
