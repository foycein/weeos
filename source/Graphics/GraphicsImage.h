#pragma once

#include "Graphics/ImageMemoryPool.h"
#include "Graphics/VulkanContext.h"

namespace Graphics
{

// A picture on the graphics card, with its memory from the pool and a view to read it or to draw into it.
// The pool must outlive the picture.
class GraphicsImage
{
public:
    GraphicsImage(const VulkanContext& context, ImageMemoryPool& memoryPool, std::uint32_t imageWidth, std::uint32_t imageHeight,
        VkFormat imageFormat, VkImageUsageFlags usage);
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
    ImageMemoryPool& pool;
    VkImage image = VK_NULL_HANDLE;
    ImageMemory imageMemory;
    VkImageView view = VK_NULL_HANDLE;
    std::uint32_t width;
    std::uint32_t height;
    VkFormat format;
};

}
