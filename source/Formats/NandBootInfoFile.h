#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

constexpr std::size_t NandBootInfoFileSize = 0x1020;

// The kind of launch, as the menu writes it: 2 for a title that the menu launches, 4 when the title that is launched is the menu itself,
// which then reads the arguments. The standby of WiiConnect24 launches with 8, which WeeOS has not.
constexpr Byte NandBootInfoTypeTitleLaunched = 2;
constexpr Byte NandBootInfoTypeMenuLaunched = 4;

// The file /shared2/sys/NANDBOOTINFO: what the menu or a title leaves for the title that starts next. The menu writes it when it launches
// a title (OSLaunchTitle, 0x8152F3E4): the arguments of the title, which is the id of the title itself at least, and who launched it.
struct NandBootInfo
{
    // What the launching program reports as its kind of application. The menu writes the byte of low memory 0x80003184, or 0x40.
    Byte applicationType;

    Byte titleType;
    std::uint32_t launchCode;
    std::uint64_t launcherTitleId;

    // The first one is the id of the title that was launched, as 16 hexadecimal digits.
    std::vector<std::string> arguments;
};

// The first four bytes are the sum of all the words behind them. Throws a FormatError when the size or the sum is wrong, or when the
// list of arguments does not lie inside the file.
NandBootInfo ReadNandBootInfo(ByteView bytes);

// Writes the arguments as the menu does: the texts at the end of the file, then the list of their places, then how many there are.
// Throws a FormatError when the arguments do not fit in the 0x1000 bytes that the file has for them.
Bytes WriteNandBootInfo(const NandBootInfo& info);

}
