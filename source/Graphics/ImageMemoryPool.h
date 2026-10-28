#pragma once

#include "Graphics/RangeAllocator.h"
#include "Graphics/VulkanContext.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace Graphics
{

// The memory of one picture: a part of one of the blocks of the pool.
struct ImageMemory
{
    VkDeviceMemory block = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize size = 0;
};

// The size of the blocks the pool takes from the graphics card. A card allows only 4096 memory allocations in all (that is the least
// every card must allow), so the pictures share a few big blocks instead of having an allocation each.
// A picture that is bigger than a block gets a block of its own.
constexpr VkDeviceSize ImageMemoryBlockSize = 64 * 1024 * 1024;

// Gives out the memory of the pictures of the device. Only one thread may use it.
class ImageMemoryPool
{
public:
    explicit ImageMemoryPool(const VulkanContext& graphicsContext);
    ~ImageMemoryPool();

    ImageMemoryPool(const ImageMemoryPool&) = delete;
    ImageMemoryPool& operator=(const ImageMemoryPool&) = delete;

    // Memory on the graphics card for a picture that has these requirements. A new block is taken when no block has room.
    ImageMemory Allocate(const VkMemoryRequirements& requirements);

    // Takes the memory back. A block that is empty again goes back to the graphics card, except one empty block of each kind of
    // memory, which stays so that the next picture finds room at once.
    void Free(const ImageMemory& imageMemory);

    // How many blocks the pool holds at the moment.
    std::size_t BlockCount() const;

private:
    struct Block
    {
        std::uint32_t memoryTypeIndex;
        VkDeviceSize size;
        RangeAllocator ranges;
    };

    std::optional<ImageMemory> AllocateInExistingBlock(const VkMemoryRequirements& requirements);
    ImageMemory AllocateInNewBlock(const VkMemoryRequirements& requirements);
    bool ShouldGiveBack(const Block& emptyBlock) const;

    const VulkanContext& context;

    // Found by the handle of the memory block.
    std::map<VkDeviceMemory, Block> blocks;
};

}
