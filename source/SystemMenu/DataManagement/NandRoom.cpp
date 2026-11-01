#include "SystemMenu/DataManagement/NandRoom.h"

#include <array>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::uint32_t UserAreaClusters = 0x4400;
constexpr std::uint32_t UserAreaInodes = 4000;

// The system area is what is left of the 0x8000 clusters, without the bad and reserved clusters and 0x140 clusters that stay free.
constexpr std::uint32_t SystemAreaClusters = 0x3C00;
constexpr std::uint32_t SystemAreaKeptFreeClusters = 0x140;
constexpr std::uint32_t SystemAreaInodes = 0x85F;

constexpr std::array<std::string_view, 10> UserAreaFolders = {
    "/meta",
    "/ticket",
    "/title/00010000",
    "/title/00010001",
    "/title/00010003",
    "/title/00010004",
    "/title/00010005",
    "/title/00010006",
    "/title/00010007",
    "/shared2/title",
};

std::uint32_t Remaining(std::uint32_t limit, std::uint32_t used)
{
    if (used >= limit)
        return 0;

    return limit - used;
}

}

std::uint32_t BlocksOfBytes(std::uint64_t bytes)
{
    return static_cast<std::uint32_t>((bytes + BlockBytes - 1) / BlockBytes);
}

std::uint32_t ClustersOfBytes(std::uint64_t bytes)
{
    return static_cast<std::uint32_t>((bytes + ClusterBytes - 1) / ClusterBytes);
}

NandRoom MeasureNandRoom(const Ios::FileSystemService& fileSystem)
{
    NandRoom room;
    for (std::string_view folder : UserAreaFolders)
    {
        Ios::Result<Ios::NandPath> path = Ios::NandPath::Parse(folder);
        Ios::Result<Ios::DirectoryUsage> usage = fileSystem.GetDirectoryUsage(path.Value());
        if (usage.Failed())
            continue;

        room.userClusters += usage.Value().usedClusters;
        room.userInodes += usage.Value().usedInodes;
    }

    room.freeUserClusters = Remaining(UserAreaClusters, room.userClusters);
    room.freeUserInodes = Remaining(UserAreaInodes, room.userInodes);

    bool isUserAreaOverItsLimit = room.userClusters > UserAreaClusters || room.userInodes > UserAreaInodes;
    Ios::Result<Ios::VolumeStatistics> statistics = fileSystem.GetVolumeStatistics();
    if (isUserAreaOverItsLimit || statistics.Failed())
        return room;

    const Ios::VolumeStatistics& volume = statistics.Value();
    std::uint32_t lostClusters = volume.badClusters + volume.reservedClusters + SystemAreaKeptFreeClusters;
    std::uint32_t systemClusters = Remaining(volume.usedClusters, room.userClusters);
    std::uint32_t systemInodes = Remaining(volume.usedInodes, room.userInodes);
    room.freeSystemClusters = Remaining(Remaining(SystemAreaClusters, lostClusters), systemClusters);
    room.freeSystemInodes = Remaining(SystemAreaInodes, systemInodes);
    return room;
}

std::uint32_t FreeUserBlocks(const NandRoom& room)
{
    return static_cast<std::uint32_t>(room.freeUserClusters * ClusterBytes / BlockBytes);
}

bool HasRoomFor(const NandRoom& room, const NandRoomNeeded& needed)
{
    if (needed.systemInodes > room.freeSystemInodes || needed.systemClusters > room.freeSystemClusters)
        return false;

    return needed.userInodes <= room.freeUserInodes && needed.userClusters <= room.freeUserClusters;
}

}
