#pragma once

#include "Graphics/VulkanContext.h"

namespace Graphics
{

// A few commands that run once, right now: uploading a picture, reading one back. Record into Buffer(), then SubmitAndWait().
class OneTimeCommands
{
public:
    explicit OneTimeCommands(const VulkanContext& graphicsContext);
    ~OneTimeCommands();

    OneTimeCommands(const OneTimeCommands&) = delete;
    OneTimeCommands& operator=(const OneTimeCommands&) = delete;

    VkCommandBuffer Buffer() const;

    // Ends the commands, hands them to the graphics card and waits until the card has done them all.
    void SubmitAndWait();

private:
    const VulkanContext& context;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
};

}
