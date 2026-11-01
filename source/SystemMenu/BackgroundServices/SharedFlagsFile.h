#pragma once

#include "SystemMenu/BackgroundServices/NandFileAccess.h"

#include <cstddef>

namespace SystemMenu
{

constexpr std::size_t SharedFlagsFileSize = 0x20;

// The file /shared2/sys/flags.dat (CSFlags of the original, 0x813682C4 to 0x81368708): 32 bytes for every title to read.
// Only one bit is used: bit 0 of the word at offset 4 copies the setting IPL.EULA, which says that the user has agreed to
// the user agreement of the network services. The channel grid and the SD Card Menu bring it up to date when they start.
//
// A missing file is made (only its owner may write it) and filled with zeros; a file that cannot be looked at is deleted and made again.
// Then the word is read, the bit is set or cleared, and the 32 bytes are written back.
void UpdateSharedFlagsFile(NandFileAccess& files);

}
