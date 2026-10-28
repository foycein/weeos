#include "Graphics/TextureStore.h"

#include "Graphics/GpuBuffer.h"
#include "Graphics/GraphicsError.h"
#include "Graphics/OneTimeCommands.h"
#include "Graphics/VulkanSupport.h"

#include <cstring>
#include <format>

namespace Graphics
{

namespace
{

constexpr VkFormat PixelFormat = VK_FORMAT_R8G8B8A8_UNORM;

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
{
    // One set for every texture, and one for the blank texture.
    std::uint32_t setCount = MaximumLiveTextures + 1;
    VkDescriptorPoolSize poolSize = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = setCount};
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = setCount,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    CheckVulkan(vkCreateDescriptorPool(context.Device(), &poolInfo, nullptr, &descriptorPool), "creating the descriptor pool of the textures");

    Formats::Rgba8Pixel whitePixel = {255, 255, 255, 255};
    std::unique_ptr<GpuImage> blankImage = std::make_unique<GpuImage>(
        context, 1, 1, PixelFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    UploadPixels(*blankImage, std::span<const Formats::Rgba8Pixel>(&whitePixel, 1));
    blankTexture = MakeTexture(std::move(blankImage), FindSampler(WrapMode::Clamp, WrapMode::Clamp, FilterMode::Nearest));
}

TextureStore::~TextureStore()
{
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

    std::unique_ptr<GpuImage> image = std::make_unique<GpuImage>(
        context, description.width, description.height, PixelFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    UploadPixels(*image, description.pixels);

    return Add(MakeTexture(std::move(image), sampler));
}

TextureHandle TextureStore::CreateDrawTarget(std::uint32_t width, std::uint32_t height)
{
    CheckSize(context, width, height);

    VkSampler sampler = FindSampler(WrapMode::Clamp, WrapMode::Clamp, FilterMode::Linear);

    std::unique_ptr<GpuImage> image = std::make_unique<GpuImage>(
        context, width, height, PictureFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    return Add(MakeTexture(std::move(image), sampler));
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
        {
            stillInUse.push_back(std::move(retired));
            continue;
        }

        vkFreeDescriptorSets(context.Device(), descriptorPool, 1, &retired.texture.descriptorSet);
    }

    retiredTextures = std::move(stillInUse);
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

void TextureStore::UploadPixels(const GpuImage& image, std::span<const Formats::Rgba8Pixel> pixels)
{
    GpuBuffer stagingBuffer(
        context, pixels.size_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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

Texture TextureStore::MakeTexture(std::unique_ptr<GpuImage> image, VkSampler sampler)
{
    VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &setLayout,
    };
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    CheckVulkan(vkAllocateDescriptorSets(context.Device(), &allocateInfo, &descriptorSet), "allocating the descriptor set of a texture");

    VkDescriptorImageInfo imageInfo = {
        .sampler = sampler,
        .imageView = image->View(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
    };
    vkUpdateDescriptorSets(context.Device(), 1, &write, 0, nullptr);

    Texture texture;
    texture.image = std::move(image);
    texture.descriptorSet = descriptorSet;
    return texture;
}

TextureHandle TextureStore::Add(Texture texture)
{
    if (textures.size() >= MaximumLiveTextures)
        throw GraphicsError(std::format("There are already {} textures, which is the most there can be", MaximumLiveTextures));

    ++lastTextureNumber;
    textures[lastTextureNumber] = std::move(texture);
    return TextureHandle{lastTextureNumber};
}

}
