#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/GraphicsImage.h"
#include "Graphics/ImageMemoryPool.h"
#include "Graphics/VulkanContext.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace Graphics
{

// How many descriptor sets one descriptor pool holds. A texture needs one set, and a new pool is made when all pools are full.
constexpr std::uint32_t DescriptorSetsPerPool = 512;

// A finished offscreen picture that is not used any more is kept for the next offscreen picture of the same size, as long as the
// pictures that wait together have no more than this many pixels (16 million pixels are 64 MB).
constexpr std::uint64_t MaximumIdlePixels = 16 * 1024 * 1024;

// A texture of the device: the picture, and the descriptor set the fragment shader reads it through.
struct Texture
{
    std::unique_ptr<GraphicsImage> image;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    // True from BeginOffscreen to EndOffscreen, while the picture is being drawn into the texture.
    bool isBeingDrawn = false;

    // True for a texture that a picture can be drawn into. It can be used again for another picture of the same size and filter.
    bool isDrawTarget = false;
    FilterMode drawTargetFilter = FilterMode::Linear;
};

// Makes the textures of the device and keeps them. The handle of a texture is a number that is never used again,
// so a handle of a destroyed texture is found out when it is used.
class TextureStore
{
public:
    TextureStore(const VulkanContext& graphicsContext, VkDescriptorSetLayout textureLayout);
    ~TextureStore();

    TextureStore(const TextureStore&) = delete;
    TextureStore& operator=(const TextureStore&) = delete;

    // Uploads the pixels and waits until they are on the graphics card.
    TextureHandle Create(const TextureDescription& description);

    // Makes a texture that a picture can be drawn into. The edges repeat their outermost pixels. It is a picture that was kept
    // from before when there is one of the same size and filter.
    TextureHandle CreateDrawTarget(std::uint32_t width, std::uint32_t height, FilterMode filter);

    // Throws a GraphicsError when there is no such texture.
    Texture& Get(TextureHandle handle);

    // The texture of a slot that the material does not use. The shader never reads it, but a texture has to be bound.
    VkDescriptorSet BlankDescriptorSet() const;

    // Takes the texture out of use. The graphics card may still draw pictures with it, so its memory is kept
    // until the picture with the frame number lastFrameUsing is finished.
    void Retire(TextureHandle handle, std::uint64_t lastFrameUsing);
    void ReleaseFinished(std::uint64_t finishedFrameNumber);

    // For tests: how many pictures the store has made on the graphics card, and how many blocks of memory they live in.
    std::size_t ImagesMade() const;
    std::size_t MemoryBlockCount() const;

private:
    struct RetiredTexture
    {
        Texture texture;
        std::uint64_t lastFrameUsing;
    };

    using SamplerKey = std::tuple<WrapMode, WrapMode, FilterMode>;
    using DrawTargetKey = std::tuple<std::uint32_t, std::uint32_t, FilterMode>;

    VkSampler FindSampler(WrapMode horizontalWrap, WrapMode verticalWrap, FilterMode filter);
    std::unique_ptr<GraphicsImage> MakeImage(std::uint32_t width, std::uint32_t height, VkFormat format, VkImageUsageFlags usage);
    void UploadPixels(const GraphicsImage& image, std::span<const Formats::Rgba8Pixel> pixels);
    Texture MakeTexture(std::unique_ptr<GraphicsImage> image, VkSampler sampler);
    TextureHandle Add(Texture texture);

    void AllocateDescriptorSet(Texture& texture);
    bool TryAllocateDescriptorSet(VkDescriptorPool descriptorPool, Texture& texture);
    VkDescriptorPool CreateDescriptorPool();

    std::optional<Texture> TakeIdleDrawTarget(const DrawTargetKey& key);
    bool CanKeepIdle(const Texture& texture) const;
    void KeepIdle(Texture texture);
    void Dispose(Texture texture);

    const VulkanContext& context;
    VkDescriptorSetLayout setLayout;
    std::vector<VkDescriptorPool> descriptorPools;
    std::map<SamplerKey, VkSampler> samplers;

    // The pictures take their memory from this pool, so it has to be made before them and to go away after them.
    ImageMemoryPool memoryPool;
    std::size_t imagesMade = 0;

    std::unordered_map<std::uint32_t, Texture> textures;
    std::vector<RetiredTexture> retiredTextures;
    std::map<DrawTargetKey, std::vector<Texture>> idleDrawTargets;
    std::uint64_t idlePixelCount = 0;
    std::uint32_t lastTextureNumber = 0;
    Texture blankTexture;
};

}
