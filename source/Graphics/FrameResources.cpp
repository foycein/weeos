#include "Graphics/FrameResources.h"

#include "Graphics/VulkanSupport.h"

namespace Graphics
{

namespace
{

constexpr VkMemoryPropertyFlags ReachableByTheProgram = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

}

FrameResources::FrameResources(const VulkanContext& graphicsContext, VkDescriptorSetLayout drawDataSetLayout)
    : context(graphicsContext)
    , drawRecordBuffer(context, MaximumDrawsPerPicture * sizeof(GpuDrawRecord), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
    , stageBuffer(context, MaximumStagesPerPicture * sizeof(GpuStage), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
{
    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.CommandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CheckVulkan(vkAllocateCommandBuffers(context.Device(), &allocateInfo, &commandBuffer), "allocating a command buffer");

    VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    CheckVulkan(vkCreateFence(context.Device(), &fenceInfo, nullptr, &fence), "creating a fence");

    VkDescriptorPoolSize poolSize = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2};
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    CheckVulkan(vkCreateDescriptorPool(context.Device(), &poolInfo, nullptr, &descriptorPool), "creating the descriptor pool of the draw data");

    VkDescriptorSetAllocateInfo setInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &drawDataSetLayout,
    };
    CheckVulkan(vkAllocateDescriptorSets(context.Device(), &setInfo, &descriptorSet), "allocating the descriptor set of the draw data");

    VkDescriptorBufferInfo drawRecordInfo = {.buffer = drawRecordBuffer.Handle(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkDescriptorBufferInfo stageInfo = {.buffer = stageBuffer.Handle(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkWriteDescriptorSet writes[] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &drawRecordInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &stageInfo},
    };
    vkUpdateDescriptorSets(context.Device(), 2, writes, 0, nullptr);
}

FrameResources::~FrameResources()
{
    vkDestroyDescriptorPool(context.Device(), descriptorPool, nullptr);
    vkDestroyFence(context.Device(), fence, nullptr);
    vkFreeCommandBuffers(context.Device(), context.CommandPool(), 1, &commandBuffer);
}

VkCommandBuffer FrameResources::CommandBuffer() const
{
    return commandBuffer;
}

VkFence FrameResources::Fence() const
{
    return fence;
}

VkDescriptorSet FrameResources::DescriptorSet() const
{
    return descriptorSet;
}

GpuDrawRecord* FrameResources::DrawRecords() const
{
    return static_cast<GpuDrawRecord*>(drawRecordBuffer.MappedBytes());
}

GpuStage* FrameResources::Stages() const
{
    return static_cast<GpuStage*>(stageBuffer.MappedBytes());
}

std::uint64_t FrameResources::FrameNumber() const
{
    return frameNumber;
}

void FrameResources::SetFrameNumber(std::uint64_t newFrameNumber)
{
    frameNumber = newFrameNumber;
}

}
