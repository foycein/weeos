#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Graphics
{

// How many pictures the program may record while the graphics card is still drawing the one before.
constexpr std::size_t FramesInFlight = 2;

// A wait with this limit lasts as long as it takes.
constexpr std::uint64_t NoTimeLimit = std::numeric_limits<std::uint64_t>::max();

// The format of every picture: the screen, the offscreen pictures, and the pipeline that draws to them.
// The colors are stored as they are, without any conversion for gamma.
constexpr VkFormat PictureFormat = VK_FORMAT_B8G8R8A8_UNORM;

// The format of the depth buffers that every picture has.
constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

// Throws a GraphicsError that names the action when a Vulkan call did not succeed.
void CheckVulkan(VkResult result, std::string_view action);

// Changes the layout of an image. The work before the source stage must finish and its writes in the source access
// must become visible before the destination stage can do the destination access.
struct ImageTransition
{
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    VkPipelineStageFlags2 sourceStage;
    VkAccessFlags2 sourceAccess;
    VkPipelineStageFlags2 destinationStage;
    VkAccessFlags2 destinationAccess;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, const ImageTransition& transition);

}
