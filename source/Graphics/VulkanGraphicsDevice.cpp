#include "Graphics/VulkanGraphicsDevice.h"

#include "Graphics/DrawData.h"
#include "Graphics/GpuBuffer.h"
#include "Graphics/GraphicsError.h"
#include "Graphics/MaterialCheck.h"
#include "Graphics/OneTimeCommands.h"
#include "Graphics/StageList.h"
#include "Graphics/VulkanSupport.h"

#include <algorithm>
#include <cstdint>
#include <format>

namespace Graphics
{

namespace
{

// A quad is drawn as two triangles.
constexpr std::uint32_t VerticesPerQuad = 6;

constexpr std::size_t BytesPerPixel = 4;

constexpr VkClearColorValue OpaqueBlack = {{0.0f, 0.0f, 0.0f, 1.0f}};
constexpr VkClearColorValue TransparentBlack = {{0.0f, 0.0f, 0.0f, 0.0f}};

// A new picture is drawn to as soon as the display has handed it over.
constexpr ImageTransition StartDrawing = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};

// The pictures that the drawing so far made must be seen by the drawing that goes on after another picture was drawn.
constexpr ImageTransition KeepDrawing = {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};

// A finished offscreen picture becomes a texture that the fragment shader reads.
constexpr ImageTransition FinishDrawingTexture = {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};

// A finished picture of the screen goes to the display. The semaphore of the submission does the waiting.
constexpr ImageTransition FinishDrawingScreen = {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE,
    VK_ACCESS_2_NONE};

// Reading a picture back: the copy waits for everything before it, and the texture is a texture again afterwards.
constexpr ImageTransition StartReadingBack = {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
constexpr ImageTransition FinishReadingBack = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};

VkRect2D ToVulkanRectangle(const PixelRectangle& rectangle)
{
    VkOffset2D offset = {static_cast<std::int32_t>(rectangle.left), static_cast<std::int32_t>(rectangle.top)};
    VkExtent2D size = {static_cast<std::uint32_t>(rectangle.width), static_cast<std::uint32_t>(rectangle.height)};
    return {offset, size};
}

}

VulkanGraphicsDevice::VulkanGraphicsDevice(const Platform::Window& window)
    : context(window)
    , swapchain(context)
    , pipeline(context)
    , textures(context, pipeline.TextureSetLayout())
    , standaloneResources(context, pipeline.DrawDataSetLayout())
{
    for (std::size_t frameNumber = 0; frameNumber < FramesInFlight; ++frameNumber)
        frames.push_back(std::make_unique<FrameResources>(context, pipeline.DrawDataSetLayout()));
}

VulkanGraphicsDevice::~VulkanGraphicsDevice()
{
    vkDeviceWaitIdle(context.Device());
}

TextureHandle VulkanGraphicsDevice::CreateTexture(const TextureDescription& description)
{
    return textures.Create(description);
}

void VulkanGraphicsDevice::DestroyTexture(TextureHandle texture)
{
    if (textures.Get(texture).isBeingDrawn)
        throw GraphicsError("A texture cannot be destroyed while a picture is drawn into it");

    textures.Retire(texture, lastFrameNumber);
}

void VulkanGraphicsDevice::BeginFrame(const Canvas& canvas, ScreenShape screenShape)
{
    if (isFrameActive)
        throw GraphicsError("BeginFrame was called before Present ended the picture before");

    if (offscreenTarget)
        throw GraphicsError("BeginFrame was called while an offscreen picture is being drawn");

    PrepareFrame();

    if (!swapchain.HasAcquiredImage())
    {
        isFrameActive = true;
        return;
    }

    VkExtent2D size = swapchain.Extent();
    PixelRectangle pictureArea = FitPicture({size.width, size.height}, screenShape);
    CanvasMapping mapping(canvas, pictureArea);

    ++lastFrameNumber;
    FrameResources& resources = *frames[frameIndex];
    resources.SetFrameNumber(lastFrameNumber);
    StartRecording(frameRecording, resources);

    VkImage image = swapchain.AcquiredImage();
    TransitionImage(resources.CommandBuffer(), image, StartDrawing);

    screenTarget.emplace(Target{mapping, std::nullopt, image, swapchain.AcquiredView(), size});
    BeginPass(*screenTarget, VK_ATTACHMENT_LOAD_OP_CLEAR, OpaqueBlack);
    isFrameActive = true;
}

void VulkanGraphicsDevice::SetScissor(const ScissorRectangle& scissor)
{
    RequireActivePicture("SetScissor");

    Target* target = ActiveTarget();
    if (target == nullptr)
        return;

    target->scissor = scissor;
    ApplyTargetState(*target);
}

void VulkanGraphicsDevice::ClearScissor()
{
    RequireActivePicture("ClearScissor");

    Target* target = ActiveTarget();
    if (target == nullptr)
        return;

    target->scissor.reset();
    ApplyTargetState(*target);
}

void VulkanGraphicsDevice::DrawQuad(const Quad& quad, const Material& material)
{
    RequireActivePicture("DrawQuad");
    CheckMaterial(material);
    CheckTextures(material);

    Recording* recording = CurrentRecording();
    if (recording == nullptr)
        return;

    std::vector<TevStage> stages = ChooseStages(material);

    if (recording->drawCount >= MaximumDrawsPerPicture)
        throw GraphicsError(std::format("A picture holds at most {} quads", MaximumDrawsPerPicture));

    if (recording->stageCount + stages.size() > MaximumStagesPerPicture)
        throw GraphicsError(std::format("The quads of a picture have more than the {} stages that fit", MaximumStagesPerPicture));

    std::uint32_t firstStage = recording->stageCount;
    for (const TevStage& stage : stages)
    {
        recording->resources->Stages()[recording->stageCount] = PackStage(stage);
        ++recording->stageCount;
    }

    std::uint32_t drawIndex = recording->drawCount;
    ++recording->drawCount;
    recording->resources->DrawRecords()[drawIndex] = MakeDrawRecord(quad, material, firstStage, static_cast<std::uint32_t>(stages.size()));

    BindPipeline(*recording, material.blendMode);
    BindTextures(material);

    // The draw number travels as the first instance, and the vertex shader finds the quad with it.
    vkCmdDraw(CurrentCommandBuffer(), VerticesPerQuad, 1, 0, drawIndex);
}

TextureHandle VulkanGraphicsDevice::BeginOffscreen(const Canvas& canvas, std::uint32_t width, std::uint32_t height)
{
    if (offscreenTarget)
        throw GraphicsError("BeginOffscreen was called while another offscreen picture is being drawn");

    CanvasMapping mapping(canvas, {0, 0, static_cast<float>(width), static_cast<float>(height)});

    TextureHandle handle = textures.CreateDrawTarget(width, height);
    Texture& texture = textures.Get(handle);
    texture.isBeingDrawn = true;

    // Between BeginFrame and Present the offscreen picture is part of the commands of the screen picture,
    // otherwise it gets commands of its own, which run before the call returns.
    if (frameRecording)
        PauseScreenPass();
    else
        StartRecording(standaloneRecording, standaloneResources);

    TransitionImage(CurrentCommandBuffer(), texture.image->Handle(), StartDrawing);

    offscreenTarget.emplace(Target{mapping, std::nullopt, texture.image->Handle(), texture.image->View(), {width, height}});
    offscreenTexture = handle;
    BeginPass(*offscreenTarget, VK_ATTACHMENT_LOAD_OP_CLEAR, TransparentBlack);
    return handle;
}

void VulkanGraphicsDevice::EndOffscreen()
{
    if (!offscreenTarget)
        throw GraphicsError("EndOffscreen was called but no offscreen picture is being drawn");

    EndPass();
    TransitionImage(CurrentCommandBuffer(), offscreenTarget->image, FinishDrawingTexture);

    textures.Get(offscreenTexture).isBeingDrawn = false;
    offscreenTarget.reset();

    if (standaloneRecording)
        FinishStandaloneRecording();
    else
        ResumeScreenPass();
}

void VulkanGraphicsDevice::Present()
{
    if (!isFrameActive)
        throw GraphicsError("Present was called without BeginFrame");

    if (offscreenTarget)
        throw GraphicsError("Present was called while an offscreen picture is being drawn");

    isFrameActive = false;

    if (frameRecording)
        FinishScreenPicture();
}

Formats::RgbaImage VulkanGraphicsDevice::ReadTexturePixels(TextureHandle texture)
{
    Texture& found = textures.Get(texture);
    if (found.isBeingDrawn)
        throw GraphicsError("A texture cannot be read while a picture is drawn into it");

    const GpuImage& image = *found.image;
    std::size_t pixelCount = static_cast<std::size_t>(image.Width()) * image.Height();
    GpuBuffer readBuffer(context, pixelCount * BytesPerPixel, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    OneTimeCommands commands(context);
    VkCommandBuffer commandBuffer = commands.Buffer();

    TransitionImage(commandBuffer, image.Handle(), StartReadingBack);
    VkBufferImageCopy copyRegion = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {image.Width(), image.Height(), 1},
    };
    vkCmdCopyImageToBuffer(commandBuffer, image.Handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readBuffer.Handle(), 1, &copyRegion);
    TransitionImage(commandBuffer, image.Handle(), FinishReadingBack);
    commands.SubmitAndWait();

    bool blueComesFirst = image.Format() == VK_FORMAT_B8G8R8A8_UNORM;
    std::size_t redOffset = blueComesFirst ? 2 : 0;
    std::size_t blueOffset = blueComesFirst ? 0 : 2;

    Formats::RgbaImage pixels;
    pixels.width = image.Width();
    pixels.height = image.Height();
    pixels.pixels.resize(pixelCount);

    const Formats::Byte* bytes = static_cast<const Formats::Byte*>(readBuffer.MappedBytes());
    for (std::size_t pixelNumber = 0; pixelNumber < pixelCount; ++pixelNumber)
    {
        const Formats::Byte* source = bytes + pixelNumber * BytesPerPixel;
        pixels.pixels[pixelNumber] = {source[redOffset], source[1], source[blueOffset], source[3]};
    }
    return pixels;
}

std::string VulkanGraphicsDevice::DeviceName() const
{
    return context.DeviceName();
}

bool VulkanGraphicsDevice::IsValidationActive() const
{
    return context.IsValidationActive();
}

int VulkanGraphicsDevice::ValidationMessageCount() const
{
    return context.ValidationMessageCount();
}

void VulkanGraphicsDevice::RequireActivePicture(const char* functionName) const
{
    if (!isFrameActive && !offscreenTarget)
        throw GraphicsError(std::format("{} was called while no picture is being drawn", functionName));
}

VulkanGraphicsDevice::Target* VulkanGraphicsDevice::ActiveTarget()
{
    if (offscreenTarget)
        return &*offscreenTarget;

    if (screenTarget)
        return &*screenTarget;

    return nullptr;
}

VulkanGraphicsDevice::Recording* VulkanGraphicsDevice::CurrentRecording()
{
    if (standaloneRecording)
        return &*standaloneRecording;

    if (frameRecording)
        return &*frameRecording;

    return nullptr;
}

VkCommandBuffer VulkanGraphicsDevice::CurrentCommandBuffer()
{
    return CurrentRecording()->resources->CommandBuffer();
}

void VulkanGraphicsDevice::PrepareFrame()
{
    WaitUntilFinished(*frames[frameIndex]);

    if (swapchain.IsOutOfDate())
        swapchain.Recreate();

    swapchain.AcquireImage(frameIndex);
}

void VulkanGraphicsDevice::WaitUntilFinished(FrameResources& resources)
{
    VkFence fence = resources.Fence();
    CheckVulkan(vkWaitForFences(context.Device(), 1, &fence, VK_TRUE, NoTimeLimit), "waiting for the graphics card to finish a picture");

    finishedFrameNumber = std::max(finishedFrameNumber, resources.FrameNumber());
    textures.ReleaseFinished(finishedFrameNumber);
}

void VulkanGraphicsDevice::StartRecording(std::optional<Recording>& recording, FrameResources& resources)
{
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CheckVulkan(vkBeginCommandBuffer(resources.CommandBuffer(), &beginInfo), "starting to record a picture");

    VkDescriptorSet drawDataSet = resources.DescriptorSet();
    vkCmdBindDescriptorSets(
        resources.CommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Layout(), DrawDataSetNumber, 1, &drawDataSet, 0, nullptr);

    recording = Recording{&resources};
}

void VulkanGraphicsDevice::BeginPass(const Target& target, VkAttachmentLoadOp loadOp, const VkClearColorValue& clearColor)
{
    VkRenderingAttachmentInfo colorAttachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = target.imageView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = loadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = clearColor},
    };
    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, target.size},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };
    vkCmdBeginRendering(CurrentCommandBuffer(), &renderingInfo);

    ApplyTargetState(target);
}

void VulkanGraphicsDevice::EndPass()
{
    vkCmdEndRendering(CurrentCommandBuffer());
}

void VulkanGraphicsDevice::ApplyTargetState(const Target& target)
{
    VkCommandBuffer commandBuffer = CurrentCommandBuffer();
    const PixelRectangle& pictureArea = target.mapping.PictureArea();

    VkViewport viewport = {pictureArea.left, pictureArea.top, pictureArea.width, pictureArea.height, 0.0f, 1.0f};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    PixelRectangle coveredArea = pictureArea;
    if (target.scissor)
        coveredArea = target.mapping.ScissorToPixels(*target.scissor);

    VkRect2D scissor = ToVulkanRectangle(coveredArea);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    const Canvas& canvas = target.mapping.GetCanvas();
    CanvasConstants canvasConstants = {canvas.width / 2, canvas.height / 2};
    vkCmdPushConstants(commandBuffer, pipeline.Layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(canvasConstants), &canvasConstants);
}

void VulkanGraphicsDevice::PauseScreenPass()
{
    EndPass();
    TransitionImage(CurrentCommandBuffer(), screenTarget->image, KeepDrawing);
}

void VulkanGraphicsDevice::ResumeScreenPass()
{
    BeginPass(*screenTarget, VK_ATTACHMENT_LOAD_OP_LOAD, OpaqueBlack);
}

void VulkanGraphicsDevice::FinishStandaloneRecording()
{
    FrameResources& resources = *standaloneRecording->resources;
    CheckVulkan(vkEndCommandBuffer(resources.CommandBuffer()), "finishing an offscreen picture");

    VkFence fence = resources.Fence();
    Submit(resources.CommandBuffer(), VK_NULL_HANDLE, VK_NULL_HANDLE, fence);
    CheckVulkan(vkWaitForFences(context.Device(), 1, &fence, VK_TRUE, NoTimeLimit), "waiting for an offscreen picture");

    standaloneRecording.reset();
}

void VulkanGraphicsDevice::FinishScreenPicture()
{
    FrameResources& resources = *frameRecording->resources;

    EndPass();
    TransitionImage(resources.CommandBuffer(), screenTarget->image, FinishDrawingScreen);
    CheckVulkan(vkEndCommandBuffer(resources.CommandBuffer()), "finishing the picture of the screen");

    Submit(resources.CommandBuffer(), swapchain.ImageAvailableSemaphore(frameIndex), swapchain.RenderFinishedSemaphore(), resources.Fence());
    swapchain.PresentAcquiredImage();

    frameRecording.reset();
    screenTarget.reset();

    frameIndex = (frameIndex + 1) % FramesInFlight;
    PrepareFrame();
}

void VulkanGraphicsDevice::Submit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence)
{
    VkCommandBufferSubmitInfo commandInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer,
    };
    VkSemaphoreSubmitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = waitSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSemaphoreSubmitInfo signalInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = signalSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = waitSemaphore != VK_NULL_HANDLE ? 1u : 0u,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo,
        .signalSemaphoreInfoCount = signalSemaphore != VK_NULL_HANDLE ? 1u : 0u,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    CheckVulkan(vkResetFences(context.Device(), 1, &fence), "resetting a fence");
    CheckVulkan(vkQueueSubmit2(context.Queue(), 1, &submitInfo, fence), "submitting a picture");
}

void VulkanGraphicsDevice::CheckTextures(const Material& material)
{
    for (const MaterialTexture& materialTexture : material.textures)
    {
        if (textures.Get(materialTexture.texture).isBeingDrawn)
            throw GraphicsError("A quad uses a texture while a picture is drawn into it");
    }
}

void VulkanGraphicsDevice::BindPipeline(Recording& recording, BlendMode blendMode)
{
    if (recording.boundBlendMode == blendMode)
        return;

    vkCmdBindPipeline(CurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.PipelineFor(blendMode));
    recording.boundBlendMode = blendMode;
}

void VulkanGraphicsDevice::BindTextures(const Material& material)
{
    std::array<VkDescriptorSet, MaximumTextureCount> descriptorSets;

    for (std::size_t textureNumber = 0; textureNumber < MaximumTextureCount; ++textureNumber)
    {
        if (textureNumber < material.textures.size())
            descriptorSets[textureNumber] = textures.Get(material.textures[textureNumber].texture).descriptorSet;
        else
            descriptorSets[textureNumber] = textures.BlankDescriptorSet();
    }

    vkCmdBindDescriptorSets(CurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Layout(), FirstTextureSetNumber,
        static_cast<std::uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

}
