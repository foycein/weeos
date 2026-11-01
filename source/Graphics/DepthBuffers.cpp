#include "Graphics/DepthBuffers.h"

#include "Graphics/VulkanSupport.h"

#include <algorithm>

namespace Graphics
{

DepthBuffers::DepthBuffers(const VulkanContext& graphicsContext)
    : context(graphicsContext)
    , memoryPool(graphicsContext)
{
}

const GraphicsImage& DepthBuffers::ForScreen(VkExtent2D size, std::uint64_t lastFrameUsing)
{
    return Cover(screenBuffer, size, lastFrameUsing);
}

const GraphicsImage& DepthBuffers::ForOffscreen(VkExtent2D size, std::uint64_t lastFrameUsing)
{
    return Cover(offscreenBuffer, size, lastFrameUsing);
}

void DepthBuffers::ReleaseFinished(std::uint64_t finishedFrameNumber)
{
    auto isFinished = [finishedFrameNumber](const RetiredBuffer& retired) { return retired.lastFrameUsing <= finishedFrameNumber; };
    std::erase_if(retiredBuffers, isFinished);
}

const GraphicsImage& DepthBuffers::Cover(std::unique_ptr<GraphicsImage>& buffer, VkExtent2D size, std::uint64_t lastFrameUsing)
{
    bool isBigEnough = buffer && buffer->Width() >= size.width && buffer->Height() >= size.height;
    if (isBigEnough)
        return *buffer;

    std::uint32_t width = size.width;
    std::uint32_t height = size.height;
    if (buffer)
    {
        width = std::max(width, buffer->Width());
        height = std::max(height, buffer->Height());
        retiredBuffers.push_back({std::move(buffer), lastFrameUsing});
    }

    buffer = std::make_unique<GraphicsImage>(context, memoryPool, width, height, DepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    return *buffer;
}

}
