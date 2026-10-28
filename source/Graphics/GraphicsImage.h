#pragma once

#include "Graphics/VulkanContext.h"

namespace Graphics
{

// A picture on the graphics card, with its memory and a view to read it or to draw into it.
class GraphicsImage
{
public:
    GraphicsImage(const VulkanContext& context, std::uint32_t imageWidth, std::uint32_t imageHeight, VkFormat imageFormat,
        VkImageUsageFlags usage);
    ~GraphicsImage();

    GraphicsImage(const GraphicsImage&) = delete;
    GraphicsImage& operator=(const GraphicsImage&) = delete;

    VkImage Handle() const;
    VkImageView View() const;
    std::uint32_t Width() const;
    std::uint32_t Height() const;
    VkFormat Format() const;

private:
    VkDevice device;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    std::uint32_t width;
    std::uint32_t height;
    VkFormat format;
};

}
