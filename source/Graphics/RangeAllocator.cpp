#include "Graphics/RangeAllocator.h"

#include "Formats/Alignment.h"
#include "Graphics/GraphicsError.h"

#include <format>

namespace Graphics
{

RangeAllocator::RangeAllocator(std::uint64_t memoryBlockSize)
    : blockSize(memoryBlockSize)
{
    freeRanges.push_back({0, blockSize});
}

std::optional<std::uint64_t> RangeAllocator::Allocate(std::uint64_t size, std::uint64_t alignment)
{
    std::optional<std::size_t> rangeNumber = FindSmallestRangeThatFits(size, alignment);
    if (!rangeNumber)
        return std::nullopt;

    return TakeFromRange(*rangeNumber, size, alignment);
}

std::optional<std::size_t> RangeAllocator::FindSmallestRangeThatFits(std::uint64_t size, std::uint64_t alignment) const
{
    std::optional<std::size_t> smallestRangeNumber;

    for (std::size_t rangeNumber = 0; rangeNumber < freeRanges.size(); ++rangeNumber)
    {
        const Range& range = freeRanges[rangeNumber];
        std::uint64_t alignedOffset = Formats::RoundUpToMultiple(range.offset, alignment);
        std::uint64_t rangeEnd = range.offset + range.size;
        bool fits = alignedOffset + size <= rangeEnd;
        if (!fits)
            continue;

        bool isSmallestSoFar = !smallestRangeNumber || range.size < freeRanges[*smallestRangeNumber].size;
        if (isSmallestSoFar)
            smallestRangeNumber = rangeNumber;
    }

    return smallestRangeNumber;
}

std::uint64_t RangeAllocator::TakeFromRange(std::size_t rangeNumber, std::uint64_t size, std::uint64_t alignment)
{
    Range range = freeRanges[rangeNumber];
    std::uint64_t alignedOffset = Formats::RoundUpToMultiple(range.offset, alignment);
    std::uint64_t rangeEnd = range.offset + range.size;
    std::uint64_t allocationEnd = alignedOffset + size;

    freeRanges.erase(freeRanges.begin() + static_cast<std::ptrdiff_t>(rangeNumber));

    std::size_t insertPosition = rangeNumber;
    if (alignedOffset > range.offset)
    {
        freeRanges.insert(freeRanges.begin() + static_cast<std::ptrdiff_t>(insertPosition), {range.offset, alignedOffset - range.offset});
        ++insertPosition;
    }
    if (allocationEnd < rangeEnd)
        freeRanges.insert(freeRanges.begin() + static_cast<std::ptrdiff_t>(insertPosition), {allocationEnd, rangeEnd - allocationEnd});

    return alignedOffset;
}

void RangeAllocator::Free(std::uint64_t offset, std::uint64_t size)
{
    std::size_t insertPosition = 0;
    while (insertPosition < freeRanges.size() && freeRanges[insertPosition].offset < offset)
        ++insertPosition;

    bool hasPrevious = insertPosition > 0;
    bool hasNext = insertPosition < freeRanges.size();

    std::uint64_t end = offset + size;
    bool overlapsPrevious = hasPrevious && freeRanges[insertPosition - 1].offset + freeRanges[insertPosition - 1].size > offset;
    bool overlapsNext = hasNext && freeRanges[insertPosition].offset < end;
    if (overlapsPrevious || overlapsNext || end > blockSize)
        throw GraphicsError(std::format("The memory range at {} with the size {} is not in use", offset, size));

    bool touchesPrevious = hasPrevious && freeRanges[insertPosition - 1].offset + freeRanges[insertPosition - 1].size == offset;
    bool touchesNext = hasNext && freeRanges[insertPosition].offset == end;

    if (touchesPrevious && touchesNext)
    {
        freeRanges[insertPosition - 1].size += size + freeRanges[insertPosition].size;
        freeRanges.erase(freeRanges.begin() + static_cast<std::ptrdiff_t>(insertPosition));
        return;
    }

    if (touchesPrevious)
    {
        freeRanges[insertPosition - 1].size += size;
        return;
    }

    if (touchesNext)
    {
        freeRanges[insertPosition].offset = offset;
        freeRanges[insertPosition].size += size;
        return;
    }

    freeRanges.insert(freeRanges.begin() + static_cast<std::ptrdiff_t>(insertPosition), {offset, size});
}

bool RangeAllocator::IsEntirelyFree() const
{
    return freeRanges.size() == 1 && freeRanges[0].size == blockSize;
}

}
