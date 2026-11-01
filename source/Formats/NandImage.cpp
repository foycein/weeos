#include "Formats/NandImage.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string_view>

namespace Formats
{

namespace
{

// A sector has 4096 bits, so the position of a bit fits in 12 bits.
constexpr std::uint16_t PositionMask = 0xFFF;
constexpr Byte ErasedByte = 0xFF;

std::uint64_t PageOffset(std::uint32_t pageNumber)
{
    return std::uint64_t(pageNumber) * NandPageStoredSize;
}

// The exclusive or of the positions of the bits that are set, and whether their number is odd.
struct SectorSummary
{
    std::uint16_t positionsXor = 0;
    bool hasOddNumberOfBits = false;
};

SectorSummary SummarizeSector(ByteView sector)
{
    SectorSummary summary;
    for (std::size_t byteNumber = 0; byteNumber < sector.size(); ++byteNumber)
    {
        for (int bitNumber = 0; bitNumber < BitsPerByte; ++bitNumber)
        {
            bool isSet = (sector[byteNumber] >> bitNumber) & 1;
            if (!isSet)
                continue;

            summary.positionsXor ^= static_cast<std::uint16_t>(byteNumber * BitsPerByte + bitNumber);
            summary.hasOddNumberOfBits = !summary.hasOddNumberOfBits;
        }
    }
    return summary;
}

void AppendLittleEndian(NandErrorCorrection& errorCorrection, std::size_t& position, std::uint16_t number)
{
    errorCorrection[position++] = static_cast<Byte>(number & ByteMask);
    errorCorrection[position++] = static_cast<Byte>(number >> BitsPerByte);
}

bool AreAll(ByteView bytes, Byte value)
{
    return std::all_of(bytes.begin(), bytes.end(), [value](Byte byte) { return byte == value; });
}

void RequireInside(std::uint32_t number, std::uint32_t count, std::string_view unitName)
{
    if (number < count)
        return;

    throw FormatError(std::format("NAND image: the {} {} does not exist, there are only {}", unitName, number, count));
}

}

NandErrorCorrection ComputeErrorCorrection(ByteView pageData)
{
    if (pageData.size() != NandPageDataSize)
        throw FormatError(std::format("NAND image: the error correction is made for {} bytes of page data, not for {}", NandPageDataSize, pageData.size()));

    NandErrorCorrection errorCorrection;
    std::size_t position = 0;
    for (std::size_t sectorNumber = 0; sectorNumber < NandSectorsPerPage; ++sectorNumber)
    {
        SectorSummary summary = SummarizeSector(pageData.subspan(sectorNumber * NandSectorSize, NandSectorSize));

        std::uint16_t secondNumber = summary.positionsXor;
        std::uint16_t firstNumber = summary.hasOddNumberOfBits ? (secondNumber ^ PositionMask) : secondNumber;
        AppendLittleEndian(errorCorrection, position, firstNumber);
        AppendLittleEndian(errorCorrection, position, secondNumber);
    }
    return errorCorrection;
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

NandPageCheck NandImage::CheckPageErrorCorrection(std::uint32_t pageNumber)
{
    RequireInside(pageNumber, NandPageCount, "page");
    Bytes storedPage = ReadRange(PageOffset(pageNumber), NandPageStoredSize);

    ByteView data = ByteView(storedPage).first(NandPageDataSize);
    ByteView spare = ByteView(storedPage).subspan(NandPageDataSize);
    if (AreAll(data, ErasedByte) && AreAll(spare, ErasedByte))
        return NandPageCheck::Erased;

    ByteView stored = spare.subspan(NandErrorCorrectionOffset, NandErrorCorrectionSize);
    if (AreAll(stored, ErasedByte))
        return NandPageCheck::Unprotected;

    NandErrorCorrection expected = ComputeErrorCorrection(data);
    bool belongsToTheData = std::equal(expected.begin(), expected.end(), stored.begin());
    return belongsToTheData ? NandPageCheck::Correct : NandPageCheck::Wrong;
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
