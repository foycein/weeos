#pragma once

#include "Graphics/VulkanContext.h"
#include "Graphics/VulkanSupport.h"

#include <array>
#include <optional>
#include <vector>

namespace Graphics
{

// The pictures that are shown in the window: a few images that are drawn to in turn while the display shows another one.
// The swap chain waits for the refresh of the display (vsync, the FIFO mode), and so it is the pacing of the whole program.
// While the window has no area, for example when it is minimized, there are no images.
class Swapchain
{
public:
    explicit Swapchain(const VulkanContext& graphicsContext);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    bool HasImages() const;
    VkExtent2D Extent() const;

    // True when the window has another size now, or when the display said the images no longer fit.
    bool IsOutOfDate() const;

    // Makes the images again for the size of the window. This waits until the graphics card has finished all its work.
    void Recreate();

    // Takes the next image to draw to and waits until the display has released one. This is where the program waits for the refresh.
    // The image becomes available to the first commands that wait for the semaphore of the frame.
    // Nothing happens when an image is already taken or when there are no images.
    void AcquireImage(std::size_t frameIndex);

    bool HasAcquiredImage() const;
    VkImage AcquiredImage() const;
    VkImageView AcquiredView() const;
    VkSemaphore ImageAvailableSemaphore(std::size_t frameIndex) const;

    // The semaphore that the drawing to the acquired image signals when it is done.
    VkSemaphore RenderFinishedSemaphore() const;

    // Hands the acquired image to the display, which shows it at the next refresh.
    void PresentAcquiredImage();

private:
    void CreateImages();
    void DestroyImages();
    VkSurfaceCapabilitiesKHR SurfaceCapabilities() const;
    VkSemaphore MakeSemaphore() const;

    const VulkanContext& context;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::array<VkSemaphore, FramesInFlight> imageAvailableSemaphores = {};

    std::optional<std::uint32_t> acquiredImageIndex;
    bool isStale = false;
};

}
