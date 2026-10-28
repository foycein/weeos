#pragma once

#include "Platform/Window.h"

#include <vulkan/vulkan.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace Graphics
{

// Everything that is needed to talk to one graphics card: the Vulkan instance, the surface of the window,
// the card itself, its queue and the command pool. In debug builds it also sets up the Khronos validation layer
// and counts the messages the layer prints.
class VulkanContext
{
public:
    explicit VulkanContext(const Platform::Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkInstance Instance() const;
    VkPhysicalDevice PhysicalDevice() const;
    VkDevice Device() const;
    VkSurfaceKHR Surface() const;
    VkQueue Queue() const;
    std::uint32_t QueueFamilyIndex() const;
    VkCommandPool CommandPool() const;

    // The biggest width and height of a picture.
    std::uint32_t MaximumPictureSize() const;

    // Memory on the card, or memory of the PC that the card can reach, with the properties wanted.
    VkDeviceMemory AllocateMemory(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags wantedProperties) const;

    std::string DeviceName() const;

    // The validation layer is used in debug builds when it is installed.
    bool IsValidationActive() const;

    // How many warnings and errors the validation layer has reported so far. Always zero when there is no layer.
    int ValidationMessageCount() const;

private:
    void CreateInstance();
    void CreateDebugMessenger();
    void CreateSurface(const Platform::Window& window);
    void ChoosePhysicalDevice();
    void CreateDevice();
    void CreateCommandPool();

    std::uint32_t FindMemoryType(std::uint32_t allowedTypeBits, VkMemoryPropertyFlags wantedProperties) const;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    std::uint32_t queueFamilyIndex = 0;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties deviceProperties = {};
    VkPhysicalDeviceMemoryProperties memoryProperties = {};

    bool isValidationActive = false;
    std::atomic<int> validationMessageCount = 0;
};

}
