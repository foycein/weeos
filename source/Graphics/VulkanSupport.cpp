#include "Graphics/VulkanSupport.h"

#include "Graphics/GraphicsError.h"

#include <vulkan/vk_enum_string_helper.h>

#include <format>

namespace Graphics
{

void CheckVulkan(VkResult result, std::string_view action)
{
    if (result == VK_SUCCESS)
        return;

    throw GraphicsError(std::format("Vulkan failed while {}: {}", action, string_VkResult(result)));
}

void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, const ImageTransition& transition)
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = transition.sourceStage,
        .srcAccessMask = transition.sourceAccess,
        .dstStageMask = transition.destinationStage,
        .dstAccessMask = transition.destinationAccess,
        .oldLayout = transition.oldLayout,
        .newLayout = transition.newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

}
