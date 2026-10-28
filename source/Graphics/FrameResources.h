#pragma once

#include "Graphics/DrawData.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/VulkanContext.h"

#include <cstdint>

namespace Graphics
{

// What one picture needs while the program records it and the graphics card draws it: a command buffer, a fence that
// tells when the card is done, and the two buffers with the draw data that the shaders read.
// The program records the next picture with other resources, so it never overwrites what the card is still reading.
class FrameResources
{
public:
    FrameResources(const VulkanContext& graphicsContext, VkDescriptorSetLayout drawDataSetLayout);
    ~FrameResources();

    FrameResources(const FrameResources&) = delete;
    FrameResources& operator=(const FrameResources&) = delete;

    VkCommandBuffer CommandBuffer() const;

    // Signaled while the graphics card is not working on the picture of these resources. It starts signaled.
    VkFence Fence() const;

    // The descriptor set that gives the shaders the two buffers.
    VkDescriptorSet DescriptorSet() const;

    // The memory the program fills while it records.
    ShaderDrawRecord* DrawRecords() const;
    ShaderStage* Stages() const;

    // The number of the picture that was recorded with these resources last.
    std::uint64_t FrameNumber() const;
    void SetFrameNumber(std::uint64_t newFrameNumber);

private:
    const VulkanContext& context;
    GraphicsBuffer drawRecordBuffer;
    GraphicsBuffer stageBuffer;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    std::uint64_t frameNumber = 0;
};

}
