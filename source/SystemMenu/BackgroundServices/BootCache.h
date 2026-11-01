#pragma once

#include "SystemMenu/BackgroundServices/NandFileAccess.h"

#include <cstddef>

namespace SystemMenu
{

// The boot cache keeps what the menu knew about the disc in the drive. It always has this size.
constexpr std::size_t BootCacheSize = 0xB00000;

// What the boot stage of the menu does with its boot cache at every start (main of BS2, 0x81379D38): a boot cache of another size
// is deleted, and a missing one is made again with its full size. A file that cannot be written stays as it is: the original only
// reports it. (The state file that the boot stage writes at the same time is MenuStateFile.h.)
void PrepareBootCache(NandFileAccess& files);

}
