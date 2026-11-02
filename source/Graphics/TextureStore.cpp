#include "Graphics/TextureStore.h"

#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsError.h"
#include "Graphics/OneTimeCommands.h"
#include "Graphics/VulkanSupport.h"

#include <cstring>
#include <format>
#include <limits>

namespace Graphics
{

namespace
{

constexpr VkFormat PixelFormat = VK_FORMAT_R8G8B8A8_UNORM;

// Assumed: outside of a picture that was drawn the outermost pixels repeat.
// The copy of the console reads the line above the first and the line below the last line of its frame buffer.
constexpr WrapMode DrawTargetEdgeWrap = WrapMode::Clamp;

VkSamplerAddressMode ToAddressMode(WrapMode wrapMode)
{
    switch (wrapMode)
    {
    case WrapMode::Clamp:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case WrapMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case WrapMode::Mirror:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    throw GraphicsError("A texture has a wrap mode that does not exist");
}

VkFilter ToVulkanFilter(FilterMode filter)
{
    switch (filter)
    {
    case FilterMode::Nearest:
        return VK_FILTER_NEAREST;
    case FilterMode::Linear:
        return VK_FILTER_LINEAR;
    }
    throw GraphicsError("A texture has a filter mode that does not exist");
}

std::uint64_t PixelCountOf(const Texture& texture)
{
    return static_cast<std::uint64_t>(texture.image->Width()) * texture.image->Height();
}

void CheckSize(const VulkanContext& context, std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0)
        throw GraphicsError("A texture needs a width and a height");

    std::uint32_t maximumSize = context.MaximumPictureSize();
    if (width > maximumSize || height > maximumSize)
    {
        throw GraphicsError(
            std::format("A texture of {} by {} pixels is too big for the graphics card, which allows {}", width, height, maximumSize));
    }
}

}

TextureStore::TextureStore(const VulkanContext& graphicsContext, VkDescriptorSetLayout textureLayout)
    : context(graphicsContext)
    , setLayout(textureLayout)
    , memoryPool(graphicsContext)
{
    Formats::Rgba8Pixel whitePixel = {255, 255, 255, 255};
    std::unique_ptr<GraphicsImage> blankImage = MakeImage(1, 1, PixelFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    UploadPixels(*blankImage, std::span<const Formats::Rgba8Pixel>(&whitePixel, 1));
    blankTexture = MakeTexture(std::move(blankImage), FindSampler(WrapMode::Clamp, WrapMode::Clamp, FilterMode::Nearest));
}

TextureStore::~TextureStore()
{
    for (VkDescriptorPool descriptorPool : descriptorPools)
        vkDestroyDescriptorPool(context.Device(), descriptorPool, nullptr);

    for (const auto& [key, sampler] : samplers)
        vkDestroySampler(context.Device(), sampler, nullptr);
}

TextureHandle TextureStore::Create(const TextureDescription& description)
{
    CheckSize(context, description.width, description.height);

    std::size_t pixelCount = static_cast<std::size_t>(description.width) * description.height;
    if (description.pixels.size() != pixelCount)
    {
        throw GraphicsError(std::format("A texture of {} by {} pixels needs {} pixels but {} were given", description.width,
            description.height, pixelCount, description.pixels.size()));
    }

    VkSampler sampler = FindSampler(description.horizontalWrap, description.verticalWrap, description.filter);

    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    std::unique_ptr<GraphicsImage> image = MakeImage(description.width, description.height, PixelFormat, usage);
    UploadPixels(*image, description.pixels);

    return Add(MakeTexture(std::move(image), sampler));
}

TextureHandle TextureStore::CreateDrawTarget(std::uint32_t width, std::uint32_t height, FilterMode filter)
{
    CheckSize(context, width, height);

    std::optional<Texture> idleTexture = TakeIdleDrawTarget({width, height, filter});
    if (idleTexture)
        return Add(std::move(*idleTexture));

    VkSampler sampler = FindSampler(DrawTargetEdgeWrap, DrawTargetEdgeWrap, filter);

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    Texture texture = MakeTexture(MakeImage(width, height, PictureFormat, usage), sampler);
    texture.isDrawTarget = true;
    texture.drawTargetFilter = filter;
    return Add(std::move(texture));
}

Texture& TextureStore::Get(TextureHandle handle)
{
    auto found = textures.find(handle.number);
    if (found == textures.end())
        throw GraphicsError(std::format("The texture {} does not exist or was destroyed", handle.number));

    return found->second;
}

VkDescriptorSet TextureStore::BlankDescriptorSet() const
{
    return blankTexture.descriptorSet;
}

void TextureStore::Retire(TextureHandle handle, std::uint64_t lastFrameUsing)
{
    Texture& texture = Get(handle);
    retiredTextures.push_back({std::move(texture), lastFrameUsing});
    textures.erase(handle.number);
}

void TextureStore::ReleaseFinished(std::uint64_t finishedFrameNumber)
{
    std::vector<RetiredTexture> stillInUse;

    for (RetiredTexture& retired : retiredTextures)
    {
        if (retired.lastFrameUsing > finishedFrameNumber)
            stillInUse.push_back(std::move(retired));
        else if (CanKeepIdle(retired.texture))
            KeepIdle(std::move(retired.texture));
        else
            Dispose(std::move(retired.texture));
    }

    retiredTextures = std::move(stillInUse);
}

std::size_t TextureStore::ImagesMade() const
{
    return imagesMade;
}

std::size_t TextureStore::MemoryBlockCount() const
{
    return memoryPool.BlockCount();
}

VkSampler TextureStore::FindSampler(WrapMode horizontalWrap, WrapMode verticalWrap, FilterMode filter)
{
    SamplerKey key = {horizontalWrap, verticalWrap, filter};
    auto found = samplers.find(key);
    if (found != samplers.end())
        return found->second;

    // The pictures have no smaller copies, so the sampler never has to choose between them.
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = ToVulkanFilter(filter),
        .minFilter = ToVulkanFilter(filter),
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = ToAddressMode(horizontalWrap),
        .addressModeV = ToAddressMode(verticalWrap),
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 0.0f,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    CheckVulkan(vkCreateSampler(context.Device(), &samplerInfo, nullptr, &sampler), "creating a sampler");

    samplers[key] = sampler;
    return sampler;
}

std::unique_ptr<GraphicsImage> TextureStore::MakeImage(
    std::uint32_t width, std::uint32_t height, VkFormat format, VkImageUsageFlags usage)
{
    ++imagesMade;
    return std::make_unique<GraphicsImage>(context, memoryPool, width, height, format, usage);
}

void TextureStore::UploadPixels(const GraphicsImage& image, std::span<const Formats::Rgba8Pixel> pixels)
{
    VkMemoryPropertyFlags reachableByTheProgram = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    GraphicsBuffer stagingBuffer(context, pixels.size_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, reachableByTheProgram);
    std::memcpy(stagingBuffer.MappedBytes(), pixels.data(), pixels.size_bytes());

    OneTimeCommands commands(context);
    VkCommandBuffer commandBuffer = commands.Buffer();

    TransitionImage(commandBuffer, image.Handle(),
        {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    // The pixels in the buffer follow each other row by row, without gaps.
    VkBufferImageCopy copyRegion = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {image.Width(), image.Height(), 1},
    };
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.Handle(), image.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    TransitionImage(commandBuffer, image.Handle(),
        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});

    commands.SubmitAndWait();
}

Texture TextureStore::MakeTexture(std::unique_ptr<GraphicsImage> image, VkSampler sampler)
{
    Texture texture;
    texture.image = std::move(image);
    AllocateDescriptorSet(texture);

    VkDescriptorImageInfo imageInfo = {
        .sampler = sampler,
        .imageView = texture.image->View(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = texture.descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
    };
    vkUpdateDescriptorSets(context.Device(), 1, &write, 0, nullptr);

    return texture;
}

TextureHandle TextureStore::Add(Texture texture)
{
    if (lastTextureNumber == std::numeric_limits<std::uint32_t>::max())
        throw GraphicsError("The device has used up all the texture numbers there are");

    ++lastTextureNumber;
    textures[lastTextureNumber] = std::move(texture);
    return TextureHandle{lastTextureNumber};
}

void TextureStore::AllocateDescriptorSet(Texture& texture)
{
    for (VkDescriptorPool descriptorPool : descriptorPools)
    {
        if (TryAllocateDescriptorSet(descriptorPool, texture))
            return;
    }

    VkDescriptorPool newPool = CreateDescriptorPool();
    if (!TryAllocateDescriptorSet(newPool, texture))
        throw GraphicsError("A new descriptor pool has no room for the descriptor set of a texture");
}

bool TextureStore::TryAllocateDescriptorSet(VkDescriptorPool descriptorPool, Texture& texture)
{
    VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &setLayout,
    };
    VkResult result = vkAllocateDescriptorSets(context.Device(), &allocateInfo, &texture.descriptorSet);

    bool poolIsFull = result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL;
    if (poolIsFull)
        return false;

    CheckVulkan(result, "allocating the descriptor set of a texture");
    texture.descriptorPool = descriptorPool;
    return true;
}

VkDescriptorPool TextureStore::CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = DescriptorSetsPerPool};
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = DescriptorSetsPerPool,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    CheckVulkan(vkCreateDescriptorPool(context.Device(), &poolInfo, nullptr, &descriptorPool), "creating a descriptor pool for textures");
    descriptorPools.push_back(descriptorPool);
    return descriptorPool;
}

std::optional<Texture> TextureStore::TakeIdleDrawTarget(const DrawTargetKey& key)
{
    auto found = idleDrawTargets.find(key);
    if (found == idleDrawTargets.end() || found->second.empty())
        return std::nullopt;

    Texture texture = std::move(found->second.back());
    found->second.pop_back();
    idlePixelCount -= PixelCountOf(texture);
    return texture;
}

bool TextureStore::CanKeepIdle(const Texture& texture) const
{
    return texture.isDrawTarget && idlePixelCount + PixelCountOf(texture) <= MaximumIdlePixels;
}

void TextureStore::KeepIdle(Texture texture)
{
    DrawTargetKey key = {texture.image->Width(), texture.image->Height(), texture.drawTargetFilter};
    idlePixelCount += PixelCountOf(texture);
    idleDrawTargets[key].push_back(std::move(texture));
}

void TextureStore::Dispose(Texture texture)
{
    vkFreeDescriptorSets(context.Device(), texture.descriptorPool, 1, &texture.descriptorSet);
}

}
