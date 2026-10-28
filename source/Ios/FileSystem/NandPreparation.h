#pragma once

#include "Ios/FileSystem/NandStorage.h"
#include "Ios/Result.h"

#include <cstdint>

namespace Ios
{

// The title id of the System Menu is the first entry in the list of users, so the System Menu is always user 0x1000, in group 1.
constexpr std::uint32_t SystemMenuUserId = 0x1000;
constexpr std::uint16_t SystemMenuGroupId = 1;

// The users of the WFS modules, which own the directory /wfs.
constexpr std::uint32_t WfsUserId = 0x13;
constexpr std::uint16_t WfsGroupId = 0x13;

// Does what the console does every time IOS starts, so that every program finds the NAND in the same state.
//
// The file system empties /tmp: the directory is deleted and made again when it holds anything.
// The title service then makes sure that the nine standard directories are there. A missing one is created for the super user
// with the permissions of the console. The directory /meta always belongs to the System Menu and /wfs to the WFS modules.
// Directories that exist are otherwise left as they are.
Result<void> PrepareNandFolder(NandStorage& storage);

}
