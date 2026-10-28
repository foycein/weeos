#include "Graphics/Swapchain.h"

#include "Graphics/GraphicsError.h"

#include <algorithm>

namespace Graphics
{

namespace
{

// Two images are enough: the display shows one while the program draws to the other. More would let the program
// run ahead of the display and show older pictures.
constexpr std::uint32_t WantedImageCount = 2;

void RequirePictureFormat(const VulkanContext& context)
{
    std::uint32_t formatCount = 0;
    CheckVulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(context.PhysicalDevice(), context.Surface(), &formatCount, nullptr),
        "listing the picture formats of the window");

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(context.PhysicalDevice(), context.Surface(), &formatCount, formats.data()),
        "listing the picture formats of the window");

    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == PictureFormat && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return;
    }

    throw GraphicsError("The window cannot show pictures in the format B8G8R8A8");
}

}

Swapchain::Swapchain(const VulkanContext& graphicsContext)
    : context(graphicsContext)
{
    CreateImages();
}

Swapchain::~Swapchain()
{
    DestroyImages();
}

bool Swapchain::HasImages() const
{
    return swapchain != VK_NULL_HANDLE;
}

VkExtent2D Swapchain::Extent() const
{
    return extent;
}

bool Swapchain::IsOutOfDate() const
{
    VkExtent2D surfaceExtent = SurfaceCapabilities().currentExtent;
    bool hasOtherSize = surfaceExtent.width != extent.width || surfaceExtent.height != extent.height;
    return isStale || hasOtherSize;
}

void Swapchain::Recreate()
{
    CheckVulkan(vkDeviceWaitIdle(context.Device()), "waiting for the graphics card");
    DestroyImages();
    CreateImages();
}

void Swapchain::AcquireImage(std::size_t frameIndex)
{
    while (HasImages() && !acquiredImageIndex)
    {
        std::uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(context.Device(), swapchain, NoTimeLimit, imageAvailableSemaphores[frameIndex],
            VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Recreate();
            continue;
        }

        // A suboptimal swap chain still works. It is made again before the next picture.
        if (result == VK_SUBOPTIMAL_KHR)
            isStale = true;
        else
            CheckVulkan(result, "taking the next image of the swap chain");

        acquiredImageIndex = imageIndex;
    }
}

bool Swapchain::HasAcquiredImage() const
{
    return acquiredImageIndex.has_value();
}

VkImage Swapchain::AcquiredImage() const
{
    return images[*acquiredImageIndex];
}

VkImageView Swapchain::AcquiredView() const
{
    return imageViews[*acquiredImageIndex];
}

VkSemaphore Swapchain::ImageAvailableSemaphore(std::size_t frameIndex) const
{
    return imageAvailableSemaphores[frameIndex];
}

VkSemaphore Swapchain::RenderFinishedSemaphore() const
{
    return renderFinishedSemaphores[*acquiredImageIndex];
}

void Swapchain::PresentAcquiredImage()
{
    VkSemaphore waitSemaphore = RenderFinishedSemaphore();
    std::uint32_t imageIndex = *acquiredImageIndex;

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };
    VkResult result = vkQueuePresentKHR(context.Queue(), &presentInfo);
    acquiredImageIndex.reset();

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        isStale = true;
        return;
    }

    CheckVulkan(result, "showing the picture");
}

void Swapchain::CreateImages()
{
    VkSurfaceCapabilitiesKHR capabilities = SurfaceCapabilities();
    extent = capabilities.currentExtent;
    if (extent.width == 0 || extent.height == 0)
    {
        extent = {0, 0};
        return;
    }

    RequirePictureFormat(context);
    CreateSwapchain(capabilities);
    GetImages();
    CreateImageViews();
    CreateSemaphores();
}

void Swapchain::CreateSwapchain(const VkSurfaceCapabilitiesKHR& capabilities)
{
    std::uint32_t imageCount = std::max(WantedImageCount, capabilities.minImageCount);
    bool hasMaximumImageCount = capabilities.maxImageCount > 0;
    if (hasMaximumImageCount)
        imageCount = std::min(imageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context.Surface(),
        .minImageCount = imageCount,
        .imageFormat = PictureFormat,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    CheckVulkan(vkCreateSwapchainKHR(context.Device(), &swapchainInfo, nullptr, &swapchain), "creating the swap chain");
}

void Swapchain::GetImages()
{
    std::uint32_t imageCount = 0;
    CheckVulkan(vkGetSwapchainImagesKHR(context.Device(), swapchain, &imageCount, nullptr), "counting the images of the swap chain");

    images.resize(imageCount);
    CheckVulkan(vkGetSwapchainImagesKHR(context.Device(), swapchain, &imageCount, images.data()), "getting the images of the swap chain");
}

void Swapchain::CreateImageViews()
{
    for (VkImage image : images)
    {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = PictureFormat,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        VkImageView view = VK_NULL_HANDLE;
        CheckVulkan(vkCreateImageView(context.Device(), &viewInfo, nullptr, &view), "creating the view of a swap chain image");
        imageViews.push_back(view);
    }
}

void Swapchain::CreateSemaphores()
{
    for (std::size_t imageNumber = 0; imageNumber < images.size(); ++imageNumber)
        renderFinishedSemaphores.push_back(MakeSemaphore());

    for (VkSemaphore& semaphore : imageAvailableSemaphores)
        semaphore = MakeSemaphore();
}

void Swapchain::DestroyImages()
{
    VkDevice device = context.Device();

    for (VkSemaphore& semaphore : imageAvailableSemaphores)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
        semaphore = VK_NULL_HANDLE;
    }

    for (VkSemaphore semaphore : renderFinishedSemaphores)
        vkDestroySemaphore(device, semaphore, nullptr);
    renderFinishedSemaphores.clear();

    for (VkImageView view : imageViews)
        vkDestroyImageView(device, view, nullptr);
    imageViews.clear();

    images.clear();
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
    extent = {0, 0};

    acquiredImageIndex.reset();
    isStale = false;
}

VkSurfaceCapabilitiesKHR Swapchain::SurfaceCapabilities() const
{
    VkSurfaceCapabilitiesKHR capabilities;
    CheckVulkan(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.PhysicalDevice(), context.Surface(), &capabilities),
        "asking for the size of the window");
    return capabilities;
}

VkSemaphore Swapchain::MakeSemaphore() const
{
    VkSemaphoreCreateInfo semaphoreInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    CheckVulkan(vkCreateSemaphore(context.Device(), &semaphoreInfo, nullptr, &semaphore), "creating a semaphore");
    return semaphore;
}

}
