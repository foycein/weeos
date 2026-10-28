#pragma once

#include "Ios/FileSystem/EntryPermissions.h"

#include <cstdint>

namespace Ios
{

// What the services give to the files and folders they make for the programs: everybody may read and write, and the flags byte is 0.
constexpr EntryPermissions ReadWriteForEveryone = {AccessRights::ReadWrite, AccessRights::ReadWrite, AccessRights::ReadWrite};
constexpr std::uint8_t NoEntryFlags = 0;

}
