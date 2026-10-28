#pragma once

#include "Ios/FileSystem/NandLimits.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/FileSystem/NandStorage.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace Ios
{

// Everything that the file system service and its open files share. One mutex guards all of it:
// whoever calls the methods holds the mutex, and so does whoever reads or changes the members.
//
// It is kept alive by the service and by every open file, so a file can still be closed after the service is gone.
struct FileSystemState
{
    // The rule of the file system module of the IOS about the length of a name in a look-up: see LookUpPathOf.
    FileSystemState(NandStorage nandStorage, const VolumeCapacity& volumeCapacity, bool checksNameLength);

    // The clusters and inodes that are left. The caller holds the mutex.
    std::uint32_t FreeClusterCount() const;
    std::uint32_t FreeInodeCount() const;

    // True when a file is open at this path, or at a path inside it when the path is a directory. The caller holds the mutex.
    bool IsAnyOpenInside(const NandPath& path) const;

    // The path that a look-up finds. A name of 13 characters or more finds nothing where the module checks the length of a name.
    // The module of the older IOS compares only the first 12 characters, so such a name finds the entry with these 12 characters.
    NandPath LookUpPathOf(const NandPath& path) const;

    mutable std::mutex mutex;
    NandStorage storage;
    VolumeCapacity capacity;
    bool isShutDown;
    bool checksNameLengthInLookUp;

    // The path of every open file. A file that is open twice is in the list twice.
    std::vector<NandPath> openFilePaths;
};

}
