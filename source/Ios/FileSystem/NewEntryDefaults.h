#pragma once

#include "Ios/FileSystem/EntryAttributes.h"

namespace Ios
{

// What the services give to the files and folders they make for the programs: everybody may read and write.
constexpr EntryPermissions ReadWriteForEveryone = {AccessRights::ReadWrite, AccessRights::ReadWrite, AccessRights::ReadWrite};

}
