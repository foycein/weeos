#include "Graphics/OneTimeCommands.h"

#include "Graphics/VulkanSupport.h"

namespace Graphics
{

OneTimeCommands::OneTimeCommands(const VulkanContext& graphicsContext)
    : context(graphicsContext)
{
    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.CommandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CheckVulkan(vkAllocateCommandBuffers(context.Device(), &allocateInfo, &commandBuffer), "allocating a command buffer");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CheckVulkan(vkBeginCommandBuffer(commandBuffer, &beginInfo), "starting to record commands");

    VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    CheckVulkan(vkCreateFence(context.Device(), &fenceInfo, nullptr, &fence), "creating a fence");
}

OneTimeCommands::~OneTimeCommands()
{
    vkDestroyFence(context.Device(), fence, nullptr);
    vkFreeCommandBuffers(context.Device(), context.CommandPool(), 1, &commandBuffer);
}

VkCommandBuffer OneTimeCommands::Buffer() const
{
    return commandBuffer;
}

void OneTimeCommands::SubmitAndWait()
{
    CheckVulkan(vkEndCommandBuffer(commandBuffer), "finishing the commands");

    VkCommandBufferSubmitInfo commandInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer,
    };
    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo,
    };
    CheckVulkan(vkQueueSubmit2(context.Queue(), 1, &submitInfo, fence), "submitting commands");
    CheckVulkan(vkWaitForFences(context.Device(), 1, &fence, VK_TRUE, NoTimeLimit), "waiting for the graphics card");
}

}
