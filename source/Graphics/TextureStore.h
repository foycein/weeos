#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/GraphicsImage.h"
#include "Graphics/VulkanContext.h"

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace Graphics
{

// How many textures can exist at the same time. Every texture has a memory allocation of its own,
// and a graphics card has to allow at least 4096 allocations.
constexpr std::uint32_t MaximumLiveTextures = 4000;

// A texture of the device: the picture, and the descriptor set the fragment shader reads it through.
struct Texture
{
    std::unique_ptr<GraphicsImage> image;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    // True from BeginOffscreen to EndOffscreen, while the picture is being drawn into the texture.
    bool isBeingDrawn = false;
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

    // Makes a texture that a picture can be drawn into. It has clamped edges and bilinear filtering.
    TextureHandle CreateDrawTarget(std::uint32_t width, std::uint32_t height);

    // Throws a GraphicsError when there is no such texture.
    Texture& Get(TextureHandle handle);

    // The texture of a slot that the material does not use. The shader never reads it, but a texture has to be bound.
    VkDescriptorSet BlankDescriptorSet() const;

    // Takes the texture out of use. The graphics card may still draw pictures with it, so its memory is kept
    // until the picture with the frame number lastFrameUsing is finished.
    void Retire(TextureHandle handle, std::uint64_t lastFrameUsing);
    void ReleaseFinished(std::uint64_t finishedFrameNumber);

private:
    struct RetiredTexture
    {
        Texture texture;
        std::uint64_t lastFrameUsing;
    };

    using SamplerKey = std::tuple<WrapMode, WrapMode, FilterMode>;

    VkSampler FindSampler(WrapMode horizontalWrap, WrapMode verticalWrap, FilterMode filter);
    void UploadPixels(const GraphicsImage& image, std::span<const Formats::Rgba8Pixel> pixels);
    Texture MakeTexture(std::unique_ptr<GraphicsImage> image, VkSampler sampler);
    TextureHandle Add(Texture texture);

    const VulkanContext& context;
    VkDescriptorSetLayout setLayout;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::map<SamplerKey, VkSampler> samplers;

    std::unordered_map<std::uint32_t, Texture> textures;
    std::vector<RetiredTexture> retiredTextures;
    std::uint32_t lastTextureNumber = 0;
    Texture blankTexture;
};

}
