#include "Formats/NandImage.h"

#include "Formats/FormatError.h"

#include <format>
#include <stdexcept>
#include <string_view>

namespace Formats
{

namespace
{

std::uint64_t PageOffset(std::uint32_t pageNumber)
{
    return std::uint64_t(pageNumber) * NandPageStoredSize;
}

void RequireInside(std::uint32_t number, std::uint32_t count, std::string_view unitName)
{
    if (number < count)
        return;

    throw FormatError(std::format("NAND image: the {} {} does not exist, there are only {}", unitName, number, count));
}

}

NandImage::NandImage(const std::filesystem::path& imagePath)
    : file(imagePath, std::ios::binary)
{
    if (!file)
        throw std::runtime_error(std::format("Cannot open the file {}", imagePath.string()));

    std::uintmax_t fileSize = std::filesystem::file_size(imagePath);
    if (fileSize != NandImageFileSize)
    {
        throw FormatError(std::format("NAND image: the file has {} bytes, but a BootMii backup has {}",
            fileSize, NandImageFileSize));
    }
}

Bytes NandImage::ReadPageData(std::uint32_t pageNumber)
{
    RequireInside(pageNumber, NandPageCount, "page");
    return ReadRange(PageOffset(pageNumber), NandPageDataSize);
}

Bytes NandImage::ReadPageSpare(std::uint32_t pageNumber)
{
    RequireInside(pageNumber, NandPageCount, "page");
    return ReadRange(PageOffset(pageNumber) + NandPageDataSize, NandPageSpareSize);
}

Bytes NandImage::ReadClusterData(std::uint32_t clusterNumber)
{
    RequireInside(clusterNumber, NandClusterCount, "cluster");
    return ReadPagesData(clusterNumber * NandPagesPerCluster, NandPagesPerCluster);
}

Bytes NandImage::ReadBlockData(std::uint32_t blockNumber)
{
    RequireInside(blockNumber, NandBlockCount, "block");
    return ReadPagesData(blockNumber * NandPagesPerBlock, NandPagesPerBlock);
}

Bytes NandImage::ReadBoot1Block()
{
    return ReadBlockData(Boot1Block);
}

Bytes NandImage::ReadBoot2Blocks()
{
    return ReadPagesData(FirstBoot2Block * NandPagesPerBlock, Boot2BlockCount * NandPagesPerBlock);
}

Bytes NandImage::ReadKeyBlock()
{
    return ReadRange(PageOffset(NandPageCount), NandKeyBlockSize);
}

// The pages lie one after the other, each followed by its spare bytes. This reads them in one go and keeps only the data.
Bytes NandImage::ReadPagesData(std::uint32_t firstPage, std::uint32_t pageCount)
{
    Bytes storedPages = ReadRange(PageOffset(firstPage), std::size_t(pageCount) * NandPageStoredSize);

    Bytes data;
    data.reserve(std::size_t(pageCount) * NandPageDataSize);
    for (std::uint32_t page = 0; page < pageCount; ++page)
    {
        auto pageStart = storedPages.begin() + std::size_t(page) * NandPageStoredSize;
        data.insert(data.end(), pageStart, pageStart + NandPageDataSize);
    }
    return data;
}

Bytes NandImage::ReadRange(std::uint64_t offset, std::size_t size)
{
    Bytes bytes(size);
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!file)
        throw std::runtime_error(std::format("Cannot read {} bytes at offset {:#x} of the NAND image", size, offset));

    return bytes;
}

}
