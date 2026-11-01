#pragma once

#include "Graphics/GraphicsImage.h"
#include "Graphics/ImageMemoryPool.h"
#include "Graphics/VulkanContext.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Graphics
{

// The depth buffers of the pictures: one for the pictures of the screen and one for the offscreen pictures, so that an offscreen
// picture drawn in the middle of a screen picture leaves the depth of the screen alone. Each one grows to the largest picture it
// has served (a depth buffer may be larger than its picture). A buffer that was replaced lives on until the screen pictures that
// may still use it are finished.
class DepthBuffers
{
public:
    explicit DepthBuffers(const VulkanContext& graphicsContext);

    DepthBuffers(const DepthBuffers&) = delete;
    DepthBuffers& operator=(const DepthBuffers&) = delete;

    // A depth buffer at least this big. lastFrameUsing is the number of the newest screen picture that may still use the buffer
    // that is there now.
    const GraphicsImage& ForScreen(VkExtent2D size, std::uint64_t lastFrameUsing);
    const GraphicsImage& ForOffscreen(VkExtent2D size, std::uint64_t lastFrameUsing);

    // Lets go of the replaced buffers that no unfinished picture uses.
    void ReleaseFinished(std::uint64_t finishedFrameNumber);

private:
    struct RetiredBuffer
    {
        std::unique_ptr<GraphicsImage> image;
        std::uint64_t lastFrameUsing;
    };

    const GraphicsImage& Cover(std::unique_ptr<GraphicsImage>& buffer, VkExtent2D size, std::uint64_t lastFrameUsing);

    const VulkanContext& context;

    // The buffers take their memory from this pool, so it has to be made before them and to go away after them.
    ImageMemoryPool memoryPool;
    std::unique_ptr<GraphicsImage> screenBuffer;
    std::unique_ptr<GraphicsImage> offscreenBuffer;
    std::vector<RetiredBuffer> retiredBuffers;
};

}
