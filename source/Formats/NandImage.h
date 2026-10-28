#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace Formats
{

// A page is what the NAND reads at once, a cluster (8 pages) is what the file system stores,
// and a block (64 pages) is what the NAND erases at once.
constexpr std::size_t NandPageDataSize = 2048;
constexpr std::size_t NandPageSpareSize = 64;
constexpr std::size_t NandPageStoredSize = NandPageDataSize + NandPageSpareSize;
constexpr std::uint32_t NandPagesPerCluster = 8;
constexpr std::uint32_t NandPagesPerBlock = 64;
constexpr std::uint32_t NandBlockCount = 4096;
constexpr std::uint32_t NandPageCount = NandBlockCount * NandPagesPerBlock;
constexpr std::uint32_t NandClusterCount = NandPageCount / NandPagesPerCluster;
constexpr std::size_t NandClusterSize = NandPagesPerCluster * NandPageDataSize;
constexpr std::size_t NandBlockSize = NandPagesPerBlock * NandPageDataSize;

// A BootMii backup stores every page with its spare bytes and ends with 1024 bytes that hold the keys of the console.
constexpr std::size_t NandKeyBlockSize = 1024;
constexpr std::uint64_t NandImageFileSize = std::uint64_t(NandPageCount) * NandPageStoredSize + NandKeyBlockSize;

// Block 0 holds boot1. Blocks 1 to 7 hold boot2, as two copies with their block maps. The file system starts behind them.
constexpr std::uint32_t Boot1Block = 0;
constexpr std::uint32_t FirstBoot2Block = 1;
constexpr std::uint32_t Boot2BlockCount = 7;

// A BootMii backup of a whole NAND. Everything is given as it is stored: the file system data is still encrypted.
// Reading changes the position in the file, so the image is not safe to use from several threads at once.
class NandImage
{
public:
    // Throws a FormatError when the file does not have the size of a BootMii backup.
    explicit NandImage(const std::filesystem::path& imagePath);

    // The 2048 data bytes of a page.
    Bytes ReadPageData(std::uint32_t pageNumber);

    // The 64 spare bytes of a page: the mark of a bad block, the error correction and, in file system clusters, the HMAC.
    Bytes ReadPageSpare(std::uint32_t pageNumber);

    // The 16 KiB of data of a cluster.
    Bytes ReadClusterData(std::uint32_t clusterNumber);

    // The 128 KiB of data of a block.
    Bytes ReadBlockData(std::uint32_t blockNumber);

    Bytes ReadBoot1Block();

    // The data of blocks 1 to 7 one after the other. boot2 is stored twice in them: docs/research/Boot2.md says where.
    Bytes ReadBoot2Blocks();

    // The last 1024 bytes of the file.
    Bytes ReadKeyBlock();

private:
    Bytes ReadPagesData(std::uint32_t firstPage, std::uint32_t pageCount);
    Bytes ReadRange(std::uint64_t offset, std::size_t size);

    std::ifstream file;
};

}
