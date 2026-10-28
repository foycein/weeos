#include "Graphics/GpuImage.h"

#include "Graphics/VulkanSupport.h"

namespace Graphics
{

GpuImage::GpuImage(const VulkanContext& context, std::uint32_t imageWidth, std::uint32_t imageHeight, VkFormat imageFormat,
    VkImageUsageFlags usage)
    : device(context.Device())
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
    memory = context.AllocateMemory(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVulkan(vkBindImageMemory(device, image, memory, 0), "binding the memory of an image");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    CheckVulkan(vkCreateImageView(device, &viewInfo, nullptr, &view), "creating the view of an image");
}

GpuImage::~GpuImage()
{
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

VkImage GpuImage::Handle() const
{
    return image;
}

VkImageView GpuImage::View() const
{
    return view;
}

std::uint32_t GpuImage::Width() const
{
    return width;
}

std::uint32_t GpuImage::Height() const
{
    return height;
}

VkFormat GpuImage::Format() const
{
    return format;
}

}
