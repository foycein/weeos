#include "Graphics/FrameResources.h"

#include "Graphics/DrawPipelines.h"
#include "Graphics/VulkanSupport.h"

#include <array>

namespace Graphics
{

namespace
{

constexpr VkMemoryPropertyFlags ReachableByTheProgram = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

}

FrameResources::FrameResources(const VulkanContext& graphicsContext, VkDescriptorSetLayout drawDataSetLayout, VkBuffer storedMeshCorners)
    : context(graphicsContext)
    , drawRecordBuffer(context, MaximumDrawsPerPicture * sizeof(ShaderDrawRecord), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
    , stageBuffer(context, MaximumStagesPerPicture * sizeof(ShaderStage), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
    , meshRecordBuffer(context, MaximumMeshesPerPicture * sizeof(ShaderMeshRecord), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
    , meshCornerBuffer(
          context, MaximumMeshCornersPerPicture * sizeof(ShaderMeshCorner), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
    , boneBuffer(context, MaximumBonesPerPicture * sizeof(ShaderBone), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ReachableByTheProgram)
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

    VkDescriptorPoolSize poolSize = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = DrawDataBindingCount};
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    CheckVulkan(vkCreateDescriptorPool(context.Device(), &poolInfo, nullptr, &descriptorPool),
        "creating the descriptor pool of the draw data");

    VkDescriptorSetAllocateInfo setInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &drawDataSetLayout,
    };
    CheckVulkan(vkAllocateDescriptorSets(context.Device(), &setInfo, &descriptorSet),
        "allocating the descriptor set of the draw data");

    // The buffers in the order of their bindings.
    std::array<VkBuffer, DrawDataBindingCount> buffers = {
        drawRecordBuffer.Handle(), stageBuffer.Handle(), meshRecordBuffer.Handle(), meshCornerBuffer.Handle(), storedMeshCorners,
        boneBuffer.Handle()};

    std::array<VkDescriptorBufferInfo, DrawDataBindingCount> bufferInfos;
    std::array<VkWriteDescriptorSet, DrawDataBindingCount> writes;
    for (std::uint32_t binding = 0; binding < DrawDataBindingCount; ++binding)
    {
        bufferInfos[binding] = {.buffer = buffers[binding], .offset = 0, .range = VK_WHOLE_SIZE};
        writes[binding] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = binding,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &bufferInfos[binding],
        };
    }
    vkUpdateDescriptorSets(context.Device(), DrawDataBindingCount, writes.data(), 0, nullptr);
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

ShaderDrawRecord* FrameResources::DrawRecords() const
{
    return static_cast<ShaderDrawRecord*>(drawRecordBuffer.MappedBytes());
}

ShaderStage* FrameResources::Stages() const
{
    return static_cast<ShaderStage*>(stageBuffer.MappedBytes());
}

ShaderMeshRecord* FrameResources::MeshRecords() const
{
    return static_cast<ShaderMeshRecord*>(meshRecordBuffer.MappedBytes());
}

ShaderMeshCorner* FrameResources::MeshCorners() const
{
    return static_cast<ShaderMeshCorner*>(meshCornerBuffer.MappedBytes());
}

ShaderBone* FrameResources::Bones() const
{
    return static_cast<ShaderBone*>(boneBuffer.MappedBytes());
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
