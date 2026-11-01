#include "Graphics/VulkanContext.h"

#include "Graphics/GraphicsError.h"
#include "Graphics/VulkanSupport.h"

#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace Graphics
{

namespace
{

constexpr const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

#ifdef NDEBUG
constexpr bool ValidationIsWanted = false;
#else
constexpr bool ValidationIsWanted = true;
#endif

bool IsLayerInstalled(std::string_view layerName)
{
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for (const VkLayerProperties& layer : layers)
    {
        if (layerName == layer.layerName)
            return true;
    }
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL ReceiveDebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* messageCounter)
{
    std::atomic<int>* validationMessageCount = static_cast<std::atomic<int>*>(messageCounter);
    ++*validationMessageCount;

    std::cerr << "Vulkan validation: " << callbackData->pMessage << "\n";
    return VK_FALSE;
}

bool SupportsSwapchain(VkPhysicalDevice candidate)
{
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, extensions.data());

    for (const VkExtensionProperties& extension : extensions)
    {
        if (std::string_view(VK_KHR_SWAPCHAIN_EXTENSION_NAME) == extension.extensionName)
            return true;
    }
    return false;
}

bool HasRequiredFeatures(VkPhysicalDevice candidate)
{
    VkPhysicalDeviceVulkan13Features features13 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceFeatures2 features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features13};
    vkGetPhysicalDeviceFeatures2(candidate, &features);

    return features13.dynamicRendering == VK_TRUE && features13.synchronization2 == VK_TRUE
        && features13.shaderDemoteToHelperInvocation == VK_TRUE;
}

// A family of queues that can draw and can show pictures in the window.
std::optional<std::uint32_t> FindQueueFamily(VkPhysicalDevice candidate, VkSurfaceKHR surface)
{
    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

    for (std::uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        bool canDraw = (families[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

        VkBool32 canShow = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(candidate, familyIndex, surface, &canShow);

        if (canDraw && canShow == VK_TRUE)
            return familyIndex;
    }
    return std::nullopt;
}

// Gives the queue family to use on this card, or nothing when the card cannot be used.
std::optional<std::uint32_t> FindUsableQueueFamily(VkPhysicalDevice candidate, VkSurfaceKHR surface)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(candidate, &properties);

    if (properties.apiVersion < VK_API_VERSION_1_3)
        return std::nullopt;

    if (!SupportsSwapchain(candidate))
        return std::nullopt;

    if (!HasRequiredFeatures(candidate))
        return std::nullopt;

    return FindQueueFamily(candidate, surface);
}

int DeviceTypeScore(VkPhysicalDeviceType deviceType)
{
    if (deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return 2;

    if (deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        return 1;

    return 0;
}

}

VulkanContext::VulkanContext(const Platform::Window& window)
{
    CreateInstance();
    CreateDebugMessenger();
    CreateSurface(window);
    ChoosePhysicalDevice();
    CreateDevice();
    CreateCommandPool();
}

VulkanContext::~VulkanContext()
{
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);

    if (debugMessenger != VK_NULL_HANDLE)
    {
        auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        destroyMessenger(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}

VkInstance VulkanContext::Instance() const
{
    return instance;
}

VkPhysicalDevice VulkanContext::PhysicalDevice() const
{
    return physicalDevice;
}

VkDevice VulkanContext::Device() const
{
    return device;
}

VkSurfaceKHR VulkanContext::Surface() const
{
    return surface;
}

VkQueue VulkanContext::Queue() const
{
    return queue;
}

std::uint32_t VulkanContext::QueueFamilyIndex() const
{
    return queueFamilyIndex;
}

VkCommandPool VulkanContext::CommandPool() const
{
    return commandPool;
}

std::uint32_t VulkanContext::MaximumPictureSize() const
{
    return deviceProperties.limits.maxImageDimension2D;
}

VkDeviceMemory VulkanContext::AllocateMemory(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags wantedProperties) const
{
    std::uint32_t memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, wantedProperties);
    return AllocateMemoryOfType(memoryTypeIndex, requirements.size);
}

VkDeviceMemory VulkanContext::AllocateMemoryOfType(std::uint32_t memoryTypeIndex, VkDeviceSize size) const
{
    VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = size,
        .memoryTypeIndex = memoryTypeIndex,
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    CheckVulkan(vkAllocateMemory(device, &allocateInfo, nullptr, &memory), "allocating memory");
    return memory;
}

std::string VulkanContext::DeviceName() const
{
    return deviceProperties.deviceName;
}

bool VulkanContext::IsValidationActive() const
{
    return isValidationActive;
}

int VulkanContext::ValidationMessageCount() const
{
    return validationMessageCount;
}

void VulkanContext::CreateInstance()
{
    isValidationActive = ValidationIsWanted && IsLayerInstalled(ValidationLayerName);
    if (ValidationIsWanted && !isValidationActive)
        std::cerr << "The Vulkan validation layer is not installed, so nothing is validated.\n";

    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    std::vector<const char*> layers;
    if (isValidationActive)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        layers.push_back(ValidationLayerName);
    }

    VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "WeeOS",
        .apiVersion = VK_API_VERSION_1_3,
    };

    // The validation layer also checks that the commands wait for each other where they must.
    VkValidationFeatureEnableEXT enabledValidationFeatures[] = {VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};
    VkValidationFeaturesEXT validationFeatures = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .enabledValidationFeatureCount = 1,
        .pEnabledValidationFeatures = enabledValidationFeatures,
    };

    VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = isValidationActive ? &validationFeatures : nullptr,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    CheckVulkan(vkCreateInstance(&instanceInfo, nullptr, &instance), "creating the Vulkan instance");
}

void VulkanContext::CreateDebugMessenger()
{
    if (!isValidationActive)
        return;

    auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = ReceiveDebugMessage,
        .pUserData = &validationMessageCount,
    };
    CheckVulkan(createMessenger(instance, &messengerInfo, nullptr, &debugMessenger), "creating the validation messenger");
}

void VulkanContext::CreateSurface(const Platform::Window& window)
{
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandleW(nullptr),
        .hwnd = window.NativeHandle(),
    };
    CheckVulkan(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface), "making the surface of the window");
}

void VulkanContext::ChoosePhysicalDevice()
{
    std::uint32_t candidateCount = 0;
    CheckVulkan(vkEnumeratePhysicalDevices(instance, &candidateCount, nullptr), "listing the graphics cards");

    std::vector<VkPhysicalDevice> candidates(candidateCount);
    CheckVulkan(vkEnumeratePhysicalDevices(instance, &candidateCount, candidates.data()), "listing the graphics cards");

    int bestScore = -1;
    for (VkPhysicalDevice candidate : candidates)
    {
        std::optional<std::uint32_t> family = FindUsableQueueFamily(candidate, surface);
        if (!family)
            continue;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(candidate, &properties);

        int score = DeviceTypeScore(properties.deviceType);
        if (score <= bestScore)
            continue;

        bestScore = score;
        physicalDevice = candidate;
        queueFamilyIndex = *family;
        deviceProperties = properties;
    }

    if (physicalDevice == VK_NULL_HANDLE)
        throw GraphicsError("There is no graphics card with Vulkan 1.3 that can show pictures in the window");

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
}

void VulkanContext::CreateDevice()
{
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    // The fragment program skips transparent pixels for the alpha test, which needs the third feature.
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .shaderDemoteToHelperInvocation = VK_TRUE,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features13,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = extensions,
    };
    CheckVulkan(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device), "creating the graphics device");

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
}

void VulkanContext::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    CheckVulkan(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "creating the command pool");
}

std::uint32_t VulkanContext::FindMemoryType(std::uint32_t allowedTypeBits, VkMemoryPropertyFlags wantedProperties) const
{
    for (std::uint32_t typeIndex = 0; typeIndex < memoryProperties.memoryTypeCount; ++typeIndex)
    {
        bool isAllowed = (allowedTypeBits & (1u << typeIndex)) != 0;
        VkMemoryPropertyFlags properties = memoryProperties.memoryTypes[typeIndex].propertyFlags;
        bool hasWantedProperties = (properties & wantedProperties) == wantedProperties;

        if (isAllowed && hasWantedProperties)
            return typeIndex;
    }

    throw GraphicsError("The graphics card has no memory of the kind that is needed");
}

}
