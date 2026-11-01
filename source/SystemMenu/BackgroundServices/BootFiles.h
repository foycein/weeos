#pragma once

#include "SystemMenu/BackgroundServices/NandFileAccess.h"

#include <cstddef>

namespace SystemMenu
{

// The boot cache keeps what the menu knew about the disc in the drive. It always has this size.
constexpr std::size_t BootCacheSize = 0xB00000;

// What the boot stage of the menu does with its two files at every start, after it has read how the console was left
// (main of BS2, 0x81379D38). The state file says from now on that the menu runs, so that a power cut is noticed at the next start.
// A boot cache of another size is deleted, and a missing one is made again with its full size.
// A file that cannot be written stays as it is: the original only reports it.
void PrepareBootFiles(NandFileAccess& files);

}
