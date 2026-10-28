#include "Graphics/GraphicsImage.h"

#include "Graphics/VulkanSupport.h"

namespace Graphics
{

GraphicsImage::GraphicsImage(const VulkanContext& context, ImageMemoryPool& memoryPool, std::uint32_t imageWidth,
    std::uint32_t imageHeight, VkFormat imageFormat, VkImageUsageFlags usage)
    : device(context.Device())
    , pool(memoryPool)
    , width(imageWidth)
    , height(imageHeight)
    , format(imageFormat)
{
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    CheckVulkan(vkCreateImage(device, &imageInfo, nullptr, &image), "creating an image");

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, image, &requirements);
    imageMemory = pool.Allocate(requirements);
    CheckVulkan(vkBindImageMemory(device, image, imageMemory.block, imageMemory.offset), "binding the memory of an image");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    CheckVulkan(vkCreateImageView(device, &viewInfo, nullptr, &view), "creating the view of an image");
}

GraphicsImage::~GraphicsImage()
{
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    pool.Free(imageMemory);
}

VkImage GraphicsImage::Handle() const
{
    return image;
}

VkImageView GraphicsImage::View() const
{
    return view;
}

std::uint32_t GraphicsImage::Width() const
{
    return width;
}

std::uint32_t GraphicsImage::Height() const
{
    return height;
}

VkFormat GraphicsImage::Format() const
{
    return format;
}

}
