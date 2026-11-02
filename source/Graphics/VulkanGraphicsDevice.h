#pragma once

#include "Graphics/CanvasMapping.h"
#include "Graphics/ConsoleVideo.h"
#include "Graphics/DepthBuffers.h"
#include "Graphics/DrawPipelines.h"
#include "Graphics/FrameResources.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/StoredMeshes.h"
#include "Graphics/Swapchain.h"
#include "Graphics/TextureStore.h"
#include "Graphics/VulkanContext.h"

#include "Formats/RgbaImage.h"
#include "Platform/Window.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Graphics
{

// The graphics device on Vulkan 1.3. It draws into the window of the program with dynamic rendering, and into offscreen
// pictures. Present waits for the refresh of the display (vsync), so the pictures come at the speed of the display.
// The 16:9 canvas lies in the middle of the window; the rest of the window is the whole area that a scissor can reach.
//
// A window that is not shown has no swap chain: its pictures are drawn into a texture as large as the window, which ReadScreenPicture
// reads back, and Present does not wait for a display. Tests draw this way.
//
// While the window has no area, for example when it is minimized, BeginFrame and Present do nothing and the drawing
// is skipped. The program should wait for window messages then instead of drawing pictures in a loop.
class VulkanGraphicsDevice : public GraphicsDevice
{
public:
    // The window must outlive the device.
    explicit VulkanGraphicsDevice(const Platform::Window& programWindow);
    ~VulkanGraphicsDevice() override;

    TextureHandle CreateTexture(const TextureDescription& description) override;
    void DestroyTexture(TextureHandle texture) override;
    void BeginFrame(const Canvas& canvas) override;
    ScissorRectangle WholeArea() const override;
    float ScreenPixelsPerUnit() const override;
    void SetScissor(const ScissorRectangle& scissor) override;
    void ClearScissor() override;
    void DrawQuad(const Quad& quad, const Material& material) override;
    void DrawMesh(std::span<const MeshVertex> triangleCorners, const MeshPlacement& placement, const Material& material) override;
    MeshHandle CreateMesh(std::span<const MeshVertex> triangleCorners) override;
    void DestroyMesh(MeshHandle mesh) override;
    void DrawMesh(MeshHandle mesh, const MeshPlacement& placement, const Material& material) override;
    TextureHandle BeginOffscreen(const Canvas& canvas, std::uint32_t width, std::uint32_t height) override;
    void EndOffscreen() override;
    void Present() override;

    // How the pictures of the screen are made from the next BeginFrame on. The default is the resolution of the window. Throws while a
    // picture is being drawn.
    void SetPictureSettings(const PictureSettings& settings);

    // The last picture of the screen of a hidden window, as large as the window. Throws for a window that is shown, and before the
    // first picture.
    Formats::RgbaImage ReadScreenPicture();

    // Reads the pixels of a texture back from the graphics card, for tests and for screenshots of offscreen pictures.
    // The texture must not be drawn into at that time. An offscreen picture that was drawn between BeginFrame and
    // Present is on the card only after Present.
    Formats::RgbaImage ReadTexturePixels(TextureHandle texture);

    std::string DeviceName() const;

    // The Vulkan validation layer is used in debug builds when it is installed.
    bool IsValidationActive() const;

    // How many warnings and errors the validation layer has printed. Always zero when there is no layer.
    int ValidationMessageCount() const;

    // For tests: how many pictures the device has made on the graphics card so far, and how many blocks of memory they live in now.
    std::size_t ImagesMade() const;
    std::size_t MemoryBlockCount() const;

private:
    // Where the commands of a picture go, and how much of the draw buffers they have used.
    struct Recording
    {
        FrameResources* resources = nullptr;
        std::uint32_t drawCount = 0;
        std::uint32_t stageCount = 0;
        std::uint32_t meshCount = 0;
        std::uint32_t meshCornerCount = 0;
        std::uint32_t boneCount = 0;
        VkPipeline boundPipeline = VK_NULL_HANDLE;
    };

    // A picture that quads and meshes are drawn into: the picture of the screen or an offscreen picture, with its depth buffer.
    struct Target
    {
        CanvasMapping mapping;
        std::optional<ScissorRectangle> scissor;
        VkImage image;
        VkImageView imageView;
        VkExtent2D size;
        const GraphicsImage* depthBuffer;
    };

    // An offscreen picture that is being drawn, and the texture that holds it when it is finished.
    struct OffscreenPicture
    {
        Target target;
        TextureHandle texture;
    };

    // The picture that the frame buffer of the console is drawn into. It is copied to the screen when the picture is finished.
    struct ConsoleFrame
    {
        TextureHandle frameBuffer;
        Material copyMaterial;
        std::uint32_t copyDrawIndex;
    };

    void RequireActivePicture(const char* functionName) const;
    Target* ActiveTarget();
    Recording* CurrentRecording();
    VkCommandBuffer CurrentCommandBuffer();

    // Gets the resources and the swap chain image of the next picture ready. Present ends with this, and so it waits there.
    void PrepareFrame();
    void PrepareHiddenScreen();
    void WaitUntilFinished(FrameResources& resources);

    // The image that the picture of the screen goes into: the swap chain image of a shown window, the texture of a hidden one.
    bool HasScreenImage() const;
    VkExtent2D ScreenSize() const;
    VkImage ScreenImage();
    VkImageView ScreenView();

    // Where the canvas of the next picture of the screen lands: in the window, or in the frame buffer of the console.
    CanvasMapping MapScreenPicture(const Canvas& canvas) const;
    void StartWindowPicture(const CanvasMapping& mapping);
    void StartConsolePicture(const CanvasMapping& mapping);
    void StartRecording(std::optional<Recording>& recording, FrameResources& resources);
    void BeginPass(const Target& target, VkAttachmentLoadOp loadOp, const VkClearColorValue& clearColor);
    void EndPass();
    void ApplyTargetState(const Target& target);

    // The picture that is drawn into waits while another picture is drawn, and goes on with what it holds.
    void PausePass(const Target& target);
    void ResumePass(const Target& target);
    void FinishStandaloneRecording();
    void FinishScreenPicture();
    void ShowOnTheDisplay(FrameResources& resources);
    void KeepHiddenPicture(FrameResources& resources);
    void CopyConsoleFrameToScreen();
    void Submit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence);

    void CheckTextures(const Material& material);

    // Puts the stages and the record of the quad into the buffers of the picture. Gives the number of the draw.
    std::uint32_t WriteDrawData(Recording& recording, const Quad& quad, const Material& material);

    // Puts the record and the corners of a mesh into the buffers of the picture. Gives the number of the mesh.
    std::uint32_t WriteMeshData(Recording& recording, std::span<const MeshVertex> triangleCorners, const MeshPlacement& placement);
    std::uint32_t WriteMeshRecord(Recording& recording, const MeshPlacement& placement);

    // Draws corners of the buffer of the picture or of the stored meshes, as the record of the mesh says.
    void RecordMeshDraw(Recording& recording, std::uint32_t meshNumber, std::uint32_t firstCorner, std::uint32_t cornerCount,
        const MeshPlacement& placement, const Material& material);
    void ApplyMeshState(const MeshPlacement& placement);

    void BindPipeline(Recording& recording, VkPipeline drawPipeline);

    // The depth buffer of the screen pictures: as big as the window, and in the console mode at least as big as its frame buffer.
    const GraphicsImage& ScreenDepthBuffer();
    void BindTextures(const Material& material);

    const Platform::Window& window;
    VulkanContext context;
    std::optional<Swapchain> swapchain;
    DrawPipelines pipeline;
    TextureStore textures;
    DepthBuffers depthBuffers;
    StoredMeshes storedMeshes;

    // The pictures of the screen take turns using these. The resources for offscreen pictures that are drawn between
    // two screen pictures are separate, because the card finishes them before the call returns.
    std::vector<std::unique_ptr<FrameResources>> frames;
    FrameResources standaloneResources;
    std::size_t frameIndex = 0;

    // The frame numbers count the pictures of the screen. A finished frame is one the graphics card has drawn completely.
    std::uint64_t lastFrameNumber = 0;
    std::uint64_t finishedFrameNumber = 0;

    PictureSettings pictureSettings;
    Canvas frameCanvas;
    bool isFrameActive = false;
    std::optional<Recording> frameRecording;
    std::optional<Recording> standaloneRecording;
    std::optional<Target> screenTarget;

    // The offscreen pictures that are being drawn, the innermost last.
    std::vector<OffscreenPicture> offscreenPictures;
    std::optional<ConsoleFrame> consoleFrame;

    TextureHandle hiddenScreen;
    VkExtent2D hiddenScreenSize = {0, 0};
    bool hasShownHiddenPicture = false;
};

}
