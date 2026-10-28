#include "Ios/FileSystem/FileSystemState.h"

#include <algorithm>
#include <utility>

namespace Ios
{

FileSystemState::FileSystemState(NandStorage nandStorage, const VolumeCapacity& volumeCapacity)
    : storage(std::move(nandStorage))
    , capacity(volumeCapacity)
    , isShutDown(false)
{
}

std::uint32_t FileSystemState::FreeClusterCount() const
{
    std::uint32_t usedClusters = storage.MeasureUsage(NandPath::Root()).usedClusters;
    if (usedClusters >= capacity.clusterCount)
        return 0;

    return capacity.clusterCount - usedClusters;
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
