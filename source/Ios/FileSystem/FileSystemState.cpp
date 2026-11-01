#include "Ios/FileSystem/FileSystemState.h"

#include <algorithm>
#include <utility>

namespace Ios
{

FileSystemState::FileSystemState(NandStorage nandStorage, const VolumeCapacity& volumeCapacity, bool checksNameLength)
    : storage(std::move(nandStorage))
    , capacity(volumeCapacity)
    , isShutDown(false)
    , checksNameLengthInLookUp(checksNameLength)
{
}

NandPath FileSystemState::LookUpPathOf(const NandPath& path) const
{
    if (checksNameLengthInLookUp)
        return path;

    return path.WithNamesCutAt(MaximumNameLength);
}

std::uint32_t FileSystemState::FreeClusterCount() const
{
    std::uint32_t usedClusters = storage.MeasureUsage(NandPath::Root()).usedClusters;
    if (usedClusters >= capacity.dataClusterCount)
        return 0;

    return capacity.dataClusterCount - usedClusters;
}

std::uint32_t FileSystemState::FreeInodeCount() const
{
    std::uint32_t usedInodes = storage.MeasureUsage(NandPath::Root()).usedInodes;
    if (usedInodes >= capacity.inodeCount)
        return 0;

    return capacity.inodeCount - usedInodes;
}

bool FileSystemState::IsAnyOpenInside(const NandPath& path) const
{
    return std::any_of(openFilePaths.begin(), openFilePaths.end(),
        [&](const NandPath& openFilePath) { return openFilePath.IsInside(path); });
}

}
