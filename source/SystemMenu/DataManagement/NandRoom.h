#pragma once

#include "Ios/FileSystem/FileSystemService.h"

#include <cstdint>

namespace SystemMenu
{

// The NAND counts its space in clusters of 16 KiB. Data Management shows sizes in blocks of 128 KiB.
constexpr std::uint64_t ClusterBytes = 0x4000;
constexpr std::uint64_t BlockBytes = 0x20000;

// The size in blocks, rounded up, as Data Management shows the size of a channel or of save data.
std::uint32_t BlocksOfBytes(std::uint64_t bytes);

// The room on the NAND as the System Menu counts it (the NAND library of the SDK, 0x815677D0 and 0x81567834).
// The NAND is split in two areas. The user area holds what the user brings: the titles of the families that Data Management shows,
// their tickets and meta data, and /shared2/title. It may grow up to 0x4400 clusters (2176 blocks) and 4000 entries.
// The system area is the rest of the NAND.
struct NandRoom
{
    std::uint32_t userClusters = 0;
    std::uint32_t userInodes = 0;
    std::uint32_t freeUserClusters = 0;
    std::uint32_t freeUserInodes = 0;
    std::uint32_t freeSystemClusters = 0;
    std::uint32_t freeSystemInodes = 0;
};

NandRoom MeasureNandRoom(const Ios::FileSystemService& fileSystem);

// The free blocks of the user area, the number in the corner of Data Management.
std::uint32_t FreeUserBlocks(const NandRoom& room);

// What a copy to the NAND needs: entries and clusters of the system area (shared contents) and of the user area.
struct NandRoomNeeded
{
    std::uint32_t systemInodes = 0;
    std::uint32_t systemClusters = 0;
    std::uint32_t userInodes = 0;
    std::uint32_t userClusters = 0;
};

bool HasRoomFor(const NandRoom& room, const NandRoomNeeded& needed);

// The clusters a file of this size takes.
std::uint32_t ClustersOfBytes(std::uint64_t bytes);

}
