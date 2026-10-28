#include "Graphics/ImageMemoryPool.h"

#include "Graphics/GraphicsError.h"

#include <algorithm>

namespace Graphics
{

ImageMemoryPool::ImageMemoryPool(const VulkanContext& graphicsContext)
    : context(graphicsContext)
{
}

ImageMemoryPool::~ImageMemoryPool()
{
    for (const auto& [memory, block] : blocks)
        vkFreeMemory(context.Device(), memory, nullptr);
}

ImageMemory ImageMemoryPool::Allocate(const VkMemoryRequirements& requirements)
{
    std::optional<ImageMemory> imageMemory = AllocateInExistingBlock(requirements);
    if (imageMemory)
        return *imageMemory;

    return AllocateInNewBlock(requirements);
}

void ImageMemoryPool::Free(const ImageMemory& imageMemory)
{
    auto found = blocks.find(imageMemory.block);
    if (found == blocks.end())
        throw GraphicsError("The memory of a picture was freed in a pool that did not give it out");

    Block& block = found->second;
    block.ranges.Free(imageMemory.offset, imageMemory.size);

    if (block.ranges.IsEntirelyFree() && ShouldGiveBack(block))
    {
        vkFreeMemory(context.Device(), found->first, nullptr);
        blocks.erase(found);
    }
}

std::size_t ImageMemoryPool::BlockCount() const
{
    return blocks.size();
}

std::optional<ImageMemory> ImageMemoryPool::AllocateInExistingBlock(const VkMemoryRequirements& requirements)
{
    for (auto& [memory, block] : blocks)
    {
        bool isAllowed = (requirements.memoryTypeBits & (1u << block.memoryTypeIndex)) != 0;
        if (!isAllowed)
            continue;

        std::optional<std::uint64_t> offset = block.ranges.Allocate(requirements.size, requirements.alignment);
        if (offset)
            return ImageMemory{memory, *offset, requirements.size};
    }

    return std::nullopt;
}

ImageMemory ImageMemoryPool::AllocateInNewBlock(const VkMemoryRequirements& requirements)
{
    std::uint32_t memoryTypeIndex = context.FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceSize blockSize = std::max(ImageMemoryBlockSize, requirements.size);
    VkDeviceMemory memory = context.AllocateMemoryOfType(memoryTypeIndex, blockSize);

    Block& block = blocks.emplace(memory, Block{memoryTypeIndex, blockSize, RangeAllocator(blockSize)}).first->second;

    // The new block is empty and big enough, and its start fits every alignment, so the range is always found.
    std::uint64_t offset = block.ranges.Allocate(requirements.size, requirements.alignment).value();
    return ImageMemory{memory, offset, requirements.size};
}

bool ImageMemoryPool::ShouldGiveBack(const Block& emptyBlock) const
{
    bool isBlockOfItsOwn = emptyBlock.size > ImageMemoryBlockSize;
    if (isBlockOfItsOwn)
        return true;

    auto hasSameKindOfMemory = [&](const auto& entry) { return entry.second.memoryTypeIndex == emptyBlock.memoryTypeIndex; };
    std::ptrdiff_t blocksOfThisKind = std::count_if(blocks.begin(), blocks.end(), hasSameKindOfMemory);
    return blocksOfThisKind > 1;
}

}
