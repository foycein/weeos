#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Graphics
{

// Keeps track of which parts of a block of memory are free, so that many pictures can share one allocation of the graphics card.
// It only does arithmetic with offsets and sizes. The memory itself belongs to the caller.
class RangeAllocator
{
public:
    explicit RangeAllocator(std::uint64_t memoryBlockSize);

    // Gives the offset of a free range of the size, at a multiple of the alignment, or nothing when no free range fits.
    // The smallest free range that fits is used, which keeps the large ranges for large pictures. The size must be more than zero.
    std::optional<std::uint64_t> Allocate(std::uint64_t size, std::uint64_t alignment);

    // Takes a range back. Free ranges that touch each other are joined. Throws a GraphicsError when the range was not in use.
    void Free(std::uint64_t offset, std::uint64_t size);

    bool IsEntirelyFree() const;

private:
    struct Range
    {
        std::uint64_t offset;
        std::uint64_t size;
    };

    std::vector<Range>::iterator PositionOf(std::size_t rangeNumber);
    std::optional<std::size_t> FindSmallestRangeThatFits(std::uint64_t size, std::uint64_t alignment) const;

    // Cuts the new range out of the free range. What is left in front of and behind it stays free.
    std::uint64_t TakeFromRange(std::size_t rangeNumber, std::uint64_t size, std::uint64_t alignment);

    std::uint64_t blockSize;

    // Sorted by offset. Two free ranges never touch each other.
    std::vector<Range> freeRanges;
};

}
