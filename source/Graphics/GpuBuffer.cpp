#include "Graphics/GpuBuffer.h"

#include "Graphics/VulkanSupport.h"

namespace Graphics
{

GpuBuffer::GpuBuffer(const VulkanContext& context, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
    : device(context.Device())
    , size(bufferSize)
{
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    CheckVulkan(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "creating a buffer");

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    memory = context.AllocateMemory(requirements, memoryProperties);
    CheckVulkan(vkBindBufferMemory(device, buffer, memory, 0), "binding the memory of a buffer");

    bool canBeMapped = (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    if (canBeMapped)
        CheckVulkan(vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mappedBytes), "mapping the memory of a buffer");
}

GpuBuffer::~GpuBuffer()
{
    if (mappedBytes != nullptr)
        vkUnmapMemory(device, memory);

    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

VkBuffer GpuBuffer::Handle() const
{
    return buffer;
}

VkDeviceSize GpuBuffer::Size() const
{
    return size;
}

void* GpuBuffer::MappedBytes() const
{
    return mappedBytes;
}

}
