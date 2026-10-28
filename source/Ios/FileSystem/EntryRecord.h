#pragma once

#include "Ios/FileSystem/EntryAttributes.h"

#include <cstdint>
#include <string>

namespace Ios
{

// Everything the NAND knows about one file or directory, besides its name and its data.
struct EntryRecord
{
    EntryKind kind;
    EntryAttributes attributes;

    // Counts up with every entry that is created or moved into a directory. A directory lists its entries from the highest number down.
    std::uint64_t linkNumber;

    // The name of the entry as a file or folder on the PC. It differs from the NAND name only when Windows cannot store that name.
    // The root has no host name.
    std::string hostName;

    // Files only: the size in bytes. The index does not store it. It is read from the file on the PC.
    std::uint32_t length;
};

}
