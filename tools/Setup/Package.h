#pragma once

#include "Formats/Bytes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Setup
{

// weeossetup.exe carries the portable zip of WeeOS as its resource PACKAGE (Setup.rc.in). The uninstaller is a copy of it without the
// zip: it carries the resource PROGRAMNAMES instead, the names of what it removes from its folder.
bool ThisProgramCarriesThePackage();

// The bytes of the zip, inside the program loaded in memory.
Formats::ByteView CarriedPackage();

std::vector<std::wstring> CarriedProgramNames();

std::filesystem::path ThisProgramFile();

// Copies this program to the uninstaller file, with the list of the program's names instead of the zip.
void WriteUninstaller(const std::filesystem::path& uninstallerFile, const std::vector<std::wstring>& programNames);

}
