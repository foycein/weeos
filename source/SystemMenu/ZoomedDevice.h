#pragma once

#include "SystemMenu/TelevisionZoom.h"

#include "Graphics/GraphicsDevice.h"

namespace SystemMenu
{

// Draws through another device with every quad and every scissor moved and scaled so that one area of the picture lands on another,
// and with the alpha of every quad scaled. The screens of the Wii Menu are drawn so that the part of the layouts that the camera looks at
// fills the picture (the camera of the layouts that 0x813AB0E8 sets, Display::LookThroughCamera).
class ZoomedDevice : public Graphics::GraphicsDevice
{
public:
    // The device must outlive this object.
    explicit ZoomedDevice(Graphics::GraphicsDevice& targetDevice);

    // What is drawn into seenArea lands in shownArea, with its alpha scaled by alpha / 255, until StopMoving.
    void Show(const PictureArea& seenArea, const PictureArea& shownArea, std::uint8_t alpha);

    // Everything is drawn as it is.
    void StopMoving();

    Graphics::TextureHandle CreateTexture(const Graphics::TextureDescription& description) override;
    void DestroyTexture(Graphics::TextureHandle texture) override;
    void BeginFrame(const Graphics::Canvas& canvas, Platform::ScreenShape screenShape) override;
    void SetScissor(const Graphics::ScissorRectangle& scissor) override;
    void ClearScissor() override;
    void DrawQuad(const Graphics::Quad& quad, const Graphics::Material& material) override;

    // The lists of channels draw no meshes; a mesh is drawn as it is.
    void DrawMesh(std::span<const Graphics::MeshVertex> triangleCorners, const Graphics::MeshPlacement& placement,
        const Graphics::Material& material) override;
    Graphics::MeshHandle CreateMesh(std::span<const Graphics::MeshVertex> triangleCorners) override;
    void DestroyMesh(Graphics::MeshHandle mesh) override;
    void DrawMesh(Graphics::MeshHandle mesh, const Graphics::MeshPlacement& placement, const Graphics::Material& material) override;
    Graphics::TextureHandle BeginOffscreen(const Graphics::Canvas& canvas, std::uint32_t width, std::uint32_t height) override;
    void EndOffscreen() override;
    void Present() override;

private:
    Formats::Vector2 Moved(Formats::Vector2 point) const;

    Graphics::GraphicsDevice& target;
    PictureArea seen;
    PictureArea shown;
    std::uint8_t alpha = 255;
    bool isMoving = false;
};

}
