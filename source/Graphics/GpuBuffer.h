#pragma once

#include "Graphics/VulkanContext.h"

namespace Graphics
{

// A buffer with its own memory. A buffer in memory the PC can reach stays mapped, so the program can write to it at any time.
class GpuBuffer
{
public:
    GpuBuffer(const VulkanContext& context, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties);
    ~GpuBuffer();

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    VkBuffer Handle() const;
    VkDeviceSize Size() const;

    // The bytes of the buffer, or nothing when the memory cannot be reached from the PC.
    void* MappedBytes() const;

private:
    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size;
    void* mappedBytes = nullptr;
};

}
