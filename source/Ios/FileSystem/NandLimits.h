#pragma once

#include "Formats/Alignment.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace Ios
{

// What the console's file system allows, as the file system module of IOS80 enforces it.

// A path holds at most 63 characters. The console's buffer for a path has 64 bytes, and the last one is the end mark.
constexpr std::size_t MaximumPathLength = 63;

constexpr std::size_t MaximumNameLength = 12;

// A new directory may be at most this deep. A file may lie one level deeper, in the deepest directory.
constexpr std::size_t MaximumDirectoryDepth = 8;

// The console has 16 handles for all programs together, and the control handle of the program takes one.
constexpr std::size_t MaximumOpenFiles = 15;

// A file length is stored in 32 bits.
constexpr std::uint64_t LargestFileLength = std::numeric_limits<std::uint32_t>::max();

// Space is counted in whole clusters.
constexpr std::uint32_t ClusterSize = 0x4000;

// The number of clusters that hold a file of this length. An empty file holds none.
constexpr std::uint64_t ClustersForLength(std::uint64_t length)
{
    return Formats::DivideRoundingUp(length, ClusterSize);
}

// How much a NAND can hold. Every file and every directory, the root included, takes one inode.
struct VolumeCapacity
{
    // The clusters that can hold the data of files.
    std::uint32_t dataClusterCount;

    // Clusters the file system cannot use. The statistics report them, but they never hold data.
    std::uint32_t badClusterCount;
    std::uint32_t reservedClusterCount;

    std::uint32_t inodeCount;
};

// The reference console's NAND. The file system area has 0x7EC0 clusters (0x40 to 0x7EFF) and the file system tree has 0x17FF entries.
// The newest superblock of the dump marks 16 of the clusters as bad and 752 as reserved, which leaves 31680 clusters for data.
constexpr std::uint32_t ConsoleClusterCount = 0x7EC0;
constexpr std::uint32_t ConsoleBadClusterCount = 16;
constexpr std::uint32_t ConsoleReservedClusterCount = 752;
constexpr std::uint32_t ConsoleInodeCount = 0x17FF;

constexpr VolumeCapacity ConsoleVolumeCapacity =
{
    ConsoleClusterCount - ConsoleBadClusterCount - ConsoleReservedClusterCount,
    ConsoleBadClusterCount,
    ConsoleReservedClusterCount,
    ConsoleInodeCount,
};

}
