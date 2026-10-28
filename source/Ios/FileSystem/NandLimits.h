#pragma once

#include <cstddef>
#include <cstdint>

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

// Space is counted in whole clusters.
constexpr std::uint32_t ClusterSize = 0x4000;

// How much a NAND can hold: clusters for the data of the files, and inodes. Every file and every directory, the root included, takes one inode.
struct VolumeCapacity
{
    std::uint32_t clusterCount;
    std::uint32_t inodeCount;
};

// The console's NAND has 0x7EC0 clusters for data and 0x17FF inodes in its file system tree.
constexpr VolumeCapacity ConsoleVolumeCapacity = {0x7EC0, 0x17FF};

}
