#include "Graphics/VulkanGraphicsDevice.h"

#include "Graphics/DrawData.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsError.h"
#include "Graphics/MaterialCheck.h"
#include "Graphics/OneTimeCommands.h"
#include "Graphics/StageList.h"
#include "Graphics/VulkanSupport.h"
#include "Platform/FitPicture.h"

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

// Where the channels lie in the four bytes of a pixel: RGBA, or BGRA in the format of the screen.
constexpr std::size_t FirstByte = 0;
constexpr std::size_t GreenByte = 1;
constexpr std::size_t ThirdByte = 2;
constexpr std::size_t AlphaByte = 3;

constexpr VkClearColorValue OpaqueBlack = {{0.0f, 0.0f, 0.0f, 1.0f}};
constexpr VkClearColorValue TransparentBlack = {{0.0f, 0.0f, 0.0f, 0.0f}};

// The depth buffers: before a picture starts, the drawing of the pictures before it must be done with the buffer, which is then
// cleared. When a paused picture of the screen goes on, its depth must be complete before it is tested again.
constexpr float FarDepth = 1.0f;

constexpr VkPipelineStageFlags2 DepthTestStages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
constexpr VkAccessFlags2 DepthReadAndWrite = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

constexpr ImageTransition StartDepth = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, DepthTestStages,
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, DepthTestStages, DepthReadAndWrite, VK_IMAGE_ASPECT_DEPTH_BIT};

constexpr ImageTransition KeepDepth = {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    DepthTestStages, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, DepthTestStages, DepthReadAndWrite, VK_IMAGE_ASPECT_DEPTH_BIT};

constexpr std::size_t CornersPerTriangle = 3;

VkCullModeFlags ToVulkanCulling(FaceCulling culling)
{
    switch (culling)
    {
    case FaceCulling::None:
        return VK_CULL_MODE_NONE;
    case FaceCulling::Back:
        return VK_CULL_MODE_BACK_BIT;
    case FaceCulling::Front:
        return VK_CULL_MODE_FRONT_BIT;
    }
    return VK_CULL_MODE_NONE;
}

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

Formats::RgbaImage ToRgbaImage(const Formats::Byte* bytes, const GraphicsImage& image)
{
    bool blueComesFirst = image.Format() == VK_FORMAT_B8G8R8A8_UNORM;
    std::size_t redByte = blueComesFirst ? ThirdByte : FirstByte;
    std::size_t blueByte = blueComesFirst ? FirstByte : ThirdByte;

    Formats::RgbaImage picture;
    picture.width = image.Width();
    picture.height = image.Height();
    picture.pixels.resize(static_cast<std::size_t>(picture.width) * picture.height);

    for (std::size_t pixelNumber = 0; pixelNumber < picture.pixels.size(); ++pixelNumber)
    {
        const Formats::Byte* pixel = bytes + pixelNumber * BytesPerPixel;
        picture.pixels[pixelNumber] = {pixel[redByte], pixel[GreenByte], pixel[blueByte], pixel[AlphaByte]};
    }
    return picture;
}

VkRect2D ToVulkanRectangle(const Platform::PixelRectangle& rectangle)
{
    VkOffset2D offset = {static_cast<std::int32_t>(rectangle.left), static_cast<std::int32_t>(rectangle.top)};
    VkExtent2D size = {static_cast<std::uint32_t>(rectangle.width), static_cast<std::uint32_t>(rectangle.height)};
    return {offset, size};
}

}

VulkanGraphicsDevice::VulkanGraphicsDevice(const Platform::Window& programWindow)
    : window(programWindow)
    , context(programWindow)
    , pipeline(context)
    , textures(context, pipeline.TextureSetLayout())
    , depthBuffers(context)
    , storedMeshes(context)
    , standaloneResources(context, pipeline.DrawDataSetLayout(), storedMeshes.Buffer())
{
    for (std::size_t frameNumber = 0; frameNumber < FramesInFlight; ++frameNumber)
        frames.push_back(std::make_unique<FrameResources>(context, pipeline.DrawDataSetLayout(), storedMeshes.Buffer()));

    if (window.IsShown())
        swapchain.emplace(context);
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

void VulkanGraphicsDevice::BeginFrame(const Canvas& canvas)
{
    if (isFrameActive)
        throw GraphicsError("BeginFrame was called before Present ended the picture before");

    if (!offscreenPictures.empty())
        throw GraphicsError("BeginFrame was called while an offscreen picture is being drawn");

    frameCanvas = canvas;
    PrepareFrame();
    PrepareHiddenScreen();

    if (!HasScreenImage())
    {
        isFrameActive = true;
        return;
    }

    CanvasMapping mapping = MapScreenPicture(canvas);

    ++lastFrameNumber;
    FrameResources& resources = *frames[frameIndex];
    resources.SetFrameNumber(lastFrameNumber);
    StartRecording(frameRecording, resources);

    if (pictureSettings.resolution == PictureResolution::Console)
        StartConsolePicture(mapping);
    else
        StartWindowPicture(mapping);

    isFrameActive = true;
}

void VulkanGraphicsDevice::SetPictureSettings(const PictureSettings& settings)
{
    if (isFrameActive || !offscreenPictures.empty())
        throw GraphicsError("The picture settings cannot change while a picture is being drawn");

    pictureSettings = settings;
}

float VulkanGraphicsDevice::ScreenPixelsPerUnit() const
{
    if (pictureSettings.resolution == PictureResolution::Console)
        return static_cast<float>(ConsoleFrameHeight) / ConsoleCanvas.height;

    VkExtent2D size = ScreenSize();
    Platform::PixelRectangle picture = Platform::FitPicture({size.width, size.height});
    return picture.height / ConsoleCanvas.height;
}

ScissorRectangle VulkanGraphicsDevice::WholeArea() const
{
    if (!offscreenPictures.empty())
        return offscreenPictures.back().target.mapping.WholeCanvasArea();

    if (screenTarget)
        return screenTarget->mapping.WholeCanvasArea();

    float halfWidth = frameCanvas.width / 2;
    float halfHeight = frameCanvas.height / 2;
    return {-halfWidth, halfWidth, -halfHeight, halfHeight};
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

    std::uint32_t drawIndex = WriteDrawData(*recording, quad, material);

    BindPipeline(*recording, pipeline.QuadPipelineFor(material.blendMode));
    BindTextures(material);

    // The draw number travels as the first instance, and the vertex shader finds the quad with it.
    vkCmdDraw(CurrentCommandBuffer(), VerticesPerQuad, 1, 0, drawIndex);
}

void VulkanGraphicsDevice::DrawMesh(std::span<const MeshVertex> triangleCorners, const MeshPlacement& placement, const Material& material)
{
    RequireActivePicture("DrawMesh");
    CheckMaterial(material);
    CheckTextures(material);

    if (triangleCorners.size() % CornersPerTriangle != 0)
        throw GraphicsError(std::format("A mesh has {} corners, which is not three per triangle", triangleCorners.size()));

    Recording* recording = CurrentRecording();
    if (recording == nullptr || triangleCorners.empty())
        return;

    std::uint32_t firstCorner = recording->meshCornerCount;
    std::uint32_t meshNumber = WriteMeshData(*recording, triangleCorners, placement);
    auto cornerCount = static_cast<std::uint32_t>(triangleCorners.size());
    RecordMeshDraw(*recording, meshNumber, firstCorner, cornerCount, placement, material);
}

MeshHandle VulkanGraphicsDevice::CreateMesh(std::span<const MeshVertex> triangleCorners)
{
    if (triangleCorners.size() % CornersPerTriangle != 0)
        throw GraphicsError(std::format("A mesh has {} corners, which is not three per triangle", triangleCorners.size()));

    return storedMeshes.Create(triangleCorners);
}

void VulkanGraphicsDevice::DestroyMesh(MeshHandle mesh)
{
    storedMeshes.Retire(mesh, lastFrameNumber);
}

void VulkanGraphicsDevice::DrawMesh(MeshHandle mesh, const MeshPlacement& placement, const Material& material)
{
    RequireActivePicture("DrawMesh");
    CheckMaterial(material);
    CheckTextures(material);
    StoredMesh stored = storedMeshes.Get(mesh);

    Recording* recording = CurrentRecording();
    if (recording == nullptr)
        return;

    std::uint32_t meshNumber = WriteMeshRecord(*recording, placement);
    recording->resources->MeshRecords()[meshNumber].usesStoredCorners = 1;
    RecordMeshDraw(*recording, meshNumber, stored.firstCorner, stored.cornerCount, placement, material);
}

void VulkanGraphicsDevice::RecordMeshDraw(Recording& recording, std::uint32_t meshNumber, std::uint32_t firstCorner,
    std::uint32_t cornerCount, const MeshPlacement& placement, const Material& material)
{
    std::uint32_t drawIndex = WriteDrawData(recording, Quad(), material);
    recording.resources->DrawRecords()[drawIndex].meshNumber = meshNumber;

    BindPipeline(recording, pipeline.MeshPipelineFor(material.blendMode));
    ApplyMeshState(placement);
    BindTextures(material);

    // As for quads, the draw number travels as the first instance. The corners of the mesh start at firstCorner.
    vkCmdDraw(CurrentCommandBuffer(), cornerCount, 1, firstCorner, drawIndex);
}

TextureHandle VulkanGraphicsDevice::BeginOffscreen(const Canvas& canvas, std::uint32_t width, std::uint32_t height)
{
    CanvasMapping mapping(canvas, {0, 0, static_cast<float>(width), static_cast<float>(height)});

    TextureHandle handle = textures.CreateDrawTarget(width, height, FilterMode::Linear);
    Texture& texture = textures.Get(handle);
    texture.isBeingDrawn = true;

    // Between BeginFrame and Present the offscreen picture is part of the commands of the screen picture,
    // otherwise it gets commands of its own, which run before the call returns. A picture that is drawn inside another
    // offscreen picture is part of the commands of that one.
    if (!offscreenPictures.empty())
        PausePass(offscreenPictures.back().target);
    else if (frameRecording)
        PausePass(*screenTarget);
    else
        StartRecording(standaloneRecording, standaloneResources);

    TransitionImage(CurrentCommandBuffer(), texture.image->Handle(), StartDrawing);

    // shortcut: an inner offscreen picture uses the depth buffer of the outer one, so meshes that the outer picture draws after it
    // meet the depth of the inner picture. Give each level a depth buffer of its own when a picture with meshes holds another picture.
    const GraphicsImage& depthBuffer = depthBuffers.ForOffscreen({width, height}, lastFrameNumber);
    Target target = {mapping, std::nullopt, texture.image->Handle(), texture.image->View(), {width, height}, &depthBuffer};
    offscreenPictures.push_back({target, handle});
    BeginPass(offscreenPictures.back().target, VK_ATTACHMENT_LOAD_OP_CLEAR, TransparentBlack);
    return handle;
}

void VulkanGraphicsDevice::EndOffscreen()
{
    if (offscreenPictures.empty())
        throw GraphicsError("EndOffscreen was called but no offscreen picture is being drawn");

    EndPass();
    const OffscreenPicture& finished = offscreenPictures.back();
    TransitionImage(CurrentCommandBuffer(), finished.target.image, FinishDrawingTexture);
    textures.Get(finished.texture).isBeingDrawn = false;
    offscreenPictures.pop_back();

    if (!offscreenPictures.empty())
        ResumePass(offscreenPictures.back().target);
    else if (standaloneRecording)
        FinishStandaloneRecording();
    else
        ResumePass(*screenTarget);
}

void VulkanGraphicsDevice::Present()
{
    if (!isFrameActive)
        throw GraphicsError("Present was called without BeginFrame");

    if (!offscreenPictures.empty())
        throw GraphicsError("Present was called while an offscreen picture is being drawn");

    isFrameActive = false;

    if (frameRecording)
        FinishScreenPicture();
}

Formats::RgbaImage VulkanGraphicsDevice::ReadScreenPicture()
{
    if (swapchain)
        throw GraphicsError("The picture of a window that is shown cannot be read back");

    if (!hasShownHiddenPicture)
        throw GraphicsError("No picture of the screen has been shown yet");

    return ReadTexturePixels(hiddenScreen);
}

Formats::RgbaImage VulkanGraphicsDevice::ReadTexturePixels(TextureHandle texture)
{
    Texture& found = textures.Get(texture);
    if (found.isBeingDrawn)
        throw GraphicsError("A texture cannot be read while a picture is drawn into it");

    const GraphicsImage& image = *found.image;
    std::size_t pixelCount = static_cast<std::size_t>(image.Width()) * image.Height();
    GraphicsBuffer readBuffer(context, pixelCount * BytesPerPixel, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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

    return ToRgbaImage(static_cast<const Formats::Byte*>(readBuffer.MappedBytes()), image);
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

std::size_t VulkanGraphicsDevice::ImagesMade() const
{
    return textures.ImagesMade();
}

std::size_t VulkanGraphicsDevice::MemoryBlockCount() const
{
    return textures.MemoryBlockCount();
}

void VulkanGraphicsDevice::RequireActivePicture(const char* functionName) const
{
    if (!isFrameActive && offscreenPictures.empty())
        throw GraphicsError(std::format("{} was called while no picture is being drawn", functionName));
}

VulkanGraphicsDevice::Target* VulkanGraphicsDevice::ActiveTarget()
{
    if (!offscreenPictures.empty())
        return &offscreenPictures.back().target;

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
    if (!swapchain)
        return;

    if (swapchain->IsOutOfDate())
        swapchain->Recreate();

    swapchain->AcquireImage(frameIndex);
}

// The picture of a hidden window is drawn into a texture as large as the window, which is made again when the window changes its size.
void VulkanGraphicsDevice::PrepareHiddenScreen()
{
    if (swapchain)
        return;

    Platform::WindowSize clientSize = window.ClientSize();
    VkExtent2D size = {clientSize.width, clientSize.height};
    bool hasTheSize = hiddenScreen.IsValid() && hiddenScreenSize.width == size.width && hiddenScreenSize.height == size.height;
    if (hasTheSize)
        return;

    if (hiddenScreen.IsValid())
        textures.Retire(hiddenScreen, lastFrameNumber);

    hiddenScreen = TextureHandle();
    hiddenScreenSize = size;
    hasShownHiddenPicture = false;
    if (size.width == 0 || size.height == 0)
        return;

    hiddenScreen = textures.CreateDrawTarget(size.width, size.height, FilterMode::Linear);
}

bool VulkanGraphicsDevice::HasScreenImage() const
{
    if (swapchain)
        return swapchain->HasAcquiredImage();

    return hiddenScreen.IsValid();
}

VkExtent2D VulkanGraphicsDevice::ScreenSize() const
{
    if (swapchain)
        return swapchain->Extent();

    return hiddenScreenSize;
}

VkImage VulkanGraphicsDevice::ScreenImage()
{
    if (swapchain)
        return swapchain->AcquiredImage();

    return textures.Get(hiddenScreen).image->Handle();
}

VkImageView VulkanGraphicsDevice::ScreenView()
{
    if (swapchain)
        return swapchain->AcquiredView();

    return textures.Get(hiddenScreen).image->View();
}

void VulkanGraphicsDevice::WaitUntilFinished(FrameResources& resources)
{
    VkFence fence = resources.Fence();
    CheckVulkan(vkWaitForFences(context.Device(), 1, &fence, VK_TRUE, NoTimeLimit), "waiting for the graphics card to finish a picture");

    finishedFrameNumber = std::max(finishedFrameNumber, resources.FrameNumber());
    textures.ReleaseFinished(finishedFrameNumber);
    depthBuffers.ReleaseFinished(finishedFrameNumber);
    storedMeshes.ReleaseFinished(finishedFrameNumber);
}

CanvasMapping VulkanGraphicsDevice::MapScreenPicture(const Canvas& canvas) const
{
    if (pictureSettings.resolution == PictureResolution::Console)
        return CanvasMapping(canvas, {0, 0, static_cast<float>(ConsoleFrameWidth), static_cast<float>(ConsoleFrameHeight)});

    VkExtent2D size = ScreenSize();
    Platform::PixelRectangle wholeScreen = {0, 0, static_cast<float>(size.width), static_cast<float>(size.height)};
    return CanvasMapping(canvas, Platform::FitPicture({size.width, size.height}), wholeScreen);
}

void VulkanGraphicsDevice::StartWindowPicture(const CanvasMapping& mapping)
{
    VkImage image = ScreenImage();
    TransitionImage(CurrentCommandBuffer(), image, StartDrawing);

    screenTarget.emplace(Target{mapping, std::nullopt, image, ScreenView(), ScreenSize(), &ScreenDepthBuffer()});
    BeginPass(*screenTarget, VK_ATTACHMENT_LOAD_OP_CLEAR, OpaqueBlack);
}

void VulkanGraphicsDevice::StartConsolePicture(const CanvasMapping& mapping)
{
    TextureHandle frameBuffer = textures.CreateDrawTarget(ConsoleFrameWidth, ConsoleFrameHeight, ConsoleScalingFilter);
    Texture& texture = textures.Get(frameBuffer);
    texture.isBeingDrawn = true;

    // The copy to the screen is written now, so the picture always has room for it.
    Material copyMaterial = MakeConsoleFilterMaterial(frameBuffer, pictureSettings.consoleScan);
    std::uint32_t copyDrawIndex = WriteDrawData(*frameRecording, MakeConsolePictureQuad(), copyMaterial);
    consoleFrame = ConsoleFrame{frameBuffer, copyMaterial, copyDrawIndex};

    TransitionImage(CurrentCommandBuffer(), texture.image->Handle(), StartDrawing);

    VkExtent2D size = {ConsoleFrameWidth, ConsoleFrameHeight};
    screenTarget.emplace(Target{mapping, std::nullopt, texture.image->Handle(), texture.image->View(), size, &ScreenDepthBuffer()});
    BeginPass(*screenTarget, VK_ATTACHMENT_LOAD_OP_CLEAR, OpaqueBlack);
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
    VkImage depthImage = target.depthBuffer->Handle();
    if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        TransitionImage(CurrentCommandBuffer(), depthImage, StartDepth);
    else
        TransitionImage(CurrentCommandBuffer(), depthImage, KeepDepth);

    VkRenderingAttachmentInfo depthAttachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = target.depthBuffer->View(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = loadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.depthStencil = {FarDepth, 0}},
    };

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
        .pDepthAttachment = &depthAttachment,
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
    const Platform::PixelRectangle& pictureArea = target.mapping.PictureArea();
    const Platform::PixelRectangle& wholeArea = target.mapping.WholeArea();

    VkViewport viewport = {wholeArea.left, wholeArea.top, wholeArea.width, wholeArea.height, 0.0f, 1.0f};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    Platform::PixelRectangle coveredArea = target.mapping.CanvasPixels();
    if (target.scissor)
        coveredArea = target.mapping.ScissorToPixels(*target.scissor);

    VkRect2D scissor = ToVulkanRectangle(coveredArea);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    ScissorRectangle wholeCanvasArea = target.mapping.WholeCanvasArea();
    CanvasConstants canvasConstants;
    canvasConstants.halfPictureWidth = (wholeCanvasArea.right - wholeCanvasArea.left) / 2;
    canvasConstants.halfPictureHeight = (wholeCanvasArea.top - wholeCanvasArea.bottom) / 2;
    canvasConstants.canvasShareOfWidth = pictureArea.width / wholeArea.width;
    canvasConstants.canvasShareOfHeight = pictureArea.height / wholeArea.height;
    vkCmdPushConstants(commandBuffer, pipeline.Layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(canvasConstants), &canvasConstants);
}

void VulkanGraphicsDevice::PausePass(const Target& target)
{
    EndPass();
    TransitionImage(CurrentCommandBuffer(), target.image, KeepDrawing);
}

void VulkanGraphicsDevice::ResumePass(const Target& target)
{
    BeginPass(target, VK_ATTACHMENT_LOAD_OP_LOAD, OpaqueBlack);
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

    if (consoleFrame)
        CopyConsoleFrameToScreen();
    else
        EndPass();

    if (swapchain)
        ShowOnTheDisplay(resources);
    else
        KeepHiddenPicture(resources);

    frameRecording.reset();
    screenTarget.reset();

    frameIndex = (frameIndex + 1) % FramesInFlight;
    PrepareFrame();
}

void VulkanGraphicsDevice::ShowOnTheDisplay(FrameResources& resources)
{
    TransitionImage(resources.CommandBuffer(), swapchain->AcquiredImage(), FinishDrawingScreen);
    CheckVulkan(vkEndCommandBuffer(resources.CommandBuffer()), "finishing the picture of the screen");

    VkSemaphore imageAvailable = swapchain->ImageAvailableSemaphore(frameIndex);
    VkSemaphore renderFinished = swapchain->RenderFinishedSemaphore();
    Submit(resources.CommandBuffer(), imageAvailable, renderFinished, resources.Fence());
    swapchain->PresentAcquiredImage();
}

// The picture of a hidden window becomes a texture that ReadScreenPicture reads back.
void VulkanGraphicsDevice::KeepHiddenPicture(FrameResources& resources)
{
    TransitionImage(resources.CommandBuffer(), ScreenImage(), FinishDrawingTexture);
    CheckVulkan(vkEndCommandBuffer(resources.CommandBuffer()), "finishing the picture of the screen");

    Submit(resources.CommandBuffer(), VK_NULL_HANDLE, VK_NULL_HANDLE, resources.Fence());
    hasShownHiddenPicture = true;
}

void VulkanGraphicsDevice::CopyConsoleFrameToScreen()
{
    EndPass();

    Texture& frameBuffer = textures.Get(consoleFrame->frameBuffer);
    TransitionImage(CurrentCommandBuffer(), frameBuffer.image->Handle(), FinishDrawingTexture);
    frameBuffer.isBeingDrawn = false;

    VkExtent2D size = ScreenSize();
    Platform::PixelRectangle screenArea = Platform::FitPicture({size.width, size.height});
    VkImage image = ScreenImage();
    TransitionImage(CurrentCommandBuffer(), image, StartDrawing);

    Target copyTarget{CanvasMapping(ConsoleRasterCanvas, screenArea), std::nullopt, image, ScreenView(), size, &ScreenDepthBuffer()};
    BeginPass(copyTarget, VK_ATTACHMENT_LOAD_OP_CLEAR, OpaqueBlack);
    BindPipeline(*frameRecording, pipeline.QuadPipelineFor(consoleFrame->copyMaterial.blendMode));
    BindTextures(consoleFrame->copyMaterial);
    vkCmdDraw(CurrentCommandBuffer(), VerticesPerQuad, 1, 0, consoleFrame->copyDrawIndex);
    EndPass();

    textures.Retire(consoleFrame->frameBuffer, lastFrameNumber);
    consoleFrame.reset();
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

std::uint32_t VulkanGraphicsDevice::WriteDrawData(Recording& recording, const Quad& quad, const Material& material)
{
    std::vector<TevStage> stages = ChooseStages(material);

    if (recording.drawCount >= MaximumDrawsPerPicture)
        throw GraphicsError(std::format("A picture holds at most {} quads", MaximumDrawsPerPicture));

    if (recording.stageCount + stages.size() > MaximumStagesPerPicture)
        throw GraphicsError(std::format("The quads of a picture have more than the {} stages that fit", MaximumStagesPerPicture));

    std::uint32_t firstStage = recording.stageCount;
    for (const TevStage& stage : stages)
    {
        recording.resources->Stages()[recording.stageCount] = PackStage(stage);
        ++recording.stageCount;
    }

    std::uint32_t drawIndex = recording.drawCount;
    ++recording.drawCount;

    std::uint32_t stageCount = static_cast<std::uint32_t>(stages.size());
    recording.resources->DrawRecords()[drawIndex] = MakeDrawRecord(quad, material, firstStage, stageCount);
    return drawIndex;
}

void VulkanGraphicsDevice::CheckTextures(const Material& material)
{
    for (const MaterialTexture& materialTexture : material.textures)
    {
        if (textures.Get(materialTexture.texture).isBeingDrawn)
            throw GraphicsError("A quad uses a texture while a picture is drawn into it");
    }
}

std::uint32_t VulkanGraphicsDevice::WriteMeshData(
    Recording& recording, std::span<const MeshVertex> triangleCorners, const MeshPlacement& placement)
{
    if (recording.meshCornerCount + triangleCorners.size() > MaximumMeshCornersPerPicture)
        throw GraphicsError(std::format("The meshes of a picture have more than the {} corners that fit", MaximumMeshCornersPerPicture));

    std::uint32_t meshNumber = WriteMeshRecord(recording, placement);
    for (const MeshVertex& vertex : triangleCorners)
    {
        recording.resources->MeshCorners()[recording.meshCornerCount] = MakeMeshCorner(vertex);
        ++recording.meshCornerCount;
    }
    return meshNumber;
}

std::uint32_t VulkanGraphicsDevice::WriteMeshRecord(Recording& recording, const MeshPlacement& placement)
{
    if (recording.meshCount >= MaximumMeshesPerPicture)
        throw GraphicsError(std::format("A picture holds at most {} meshes", MaximumMeshesPerPicture));

    if (recording.boneCount + placement.boneMatrices.size() > MaximumBonesPerPicture)
        throw GraphicsError(std::format("The meshes of a picture have more than the {} bones that fit", MaximumBonesPerPicture));

    std::uint32_t meshNumber = recording.meshCount;
    ++recording.meshCount;

    ShaderMeshRecord record = MakeMeshRecord(placement);
    record.firstBone = recording.boneCount;
    record.boneCount = static_cast<std::uint32_t>(placement.boneMatrices.size());
    recording.resources->MeshRecords()[meshNumber] = record;

    for (const Matrix4& boneMatrix : placement.boneMatrices)
    {
        recording.resources->Bones()[recording.boneCount] = MakeBone(boneMatrix);
        ++recording.boneCount;
    }
    return meshNumber;
}

void VulkanGraphicsDevice::ApplyMeshState(const MeshPlacement& placement)
{
    VkBool32 writesDepth = VK_FALSE;
    if (placement.depth == DepthMode::TestAndWrite)
        writesDepth = VK_TRUE;

    VkCommandBuffer commandBuffer = CurrentCommandBuffer();
    vkCmdSetCullMode(commandBuffer, ToVulkanCulling(placement.culling));
    vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);
    vkCmdSetDepthWriteEnable(commandBuffer, writesDepth);
}

void VulkanGraphicsDevice::BindPipeline(Recording& recording, VkPipeline drawPipeline)
{
    if (recording.boundPipeline == drawPipeline)
        return;

    vkCmdBindPipeline(CurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, drawPipeline);
    recording.boundPipeline = drawPipeline;
}

const GraphicsImage& VulkanGraphicsDevice::ScreenDepthBuffer()
{
    VkExtent2D size = ScreenSize();
    if (pictureSettings.resolution == PictureResolution::Console)
    {
        size.width = std::max(size.width, ConsoleFrameWidth);
        size.height = std::max(size.height, ConsoleFrameHeight);
    }
    return depthBuffers.ForScreen(size, lastFrameNumber);
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
