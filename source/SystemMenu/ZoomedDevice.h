#pragma once

#include "SystemMenu/TelevisionZoom.h"

#include "Graphics/GraphicsDevice.h"

namespace SystemMenu
{

// Draws through another device as the camera of a zoom sees the picture: every quad and every scissor is moved and scaled so that
// the part of the picture that the zoom looks at fills the whole picture. The original sets the camera of the layouts while the
// channel grid zooms into a television (0x813AB0E8).
class ZoomedDevice : public Graphics::GraphicsDevice
{
public:
    // The device must outlive this object.
    explicit ZoomedDevice(Graphics::GraphicsDevice& targetDevice);

    // The zoom must stay alive as long as the device draws through it.
    void LookThrough(const TelevisionZoom& zoom);

    Graphics::TextureHandle CreateTexture(const Graphics::TextureDescription& description) override;
    void DestroyTexture(Graphics::TextureHandle texture) override;
    void BeginFrame(const Graphics::Canvas& canvas, Platform::ScreenShape screenShape) override;
    void SetScissor(const Graphics::ScissorRectangle& scissor) override;
    void ClearScissor() override;
    void DrawQuad(const Graphics::Quad& quad, const Graphics::Material& material) override;

    // The lists of channels draw no meshes; a mesh is drawn as it is.
    void DrawMesh(std::span<const Graphics::MeshVertex> triangleCorners, const Graphics::MeshPlacement& placement,
        const Graphics::Material& material) override;
    Graphics::TextureHandle BeginOffscreen(const Graphics::Canvas& canvas, std::uint32_t width, std::uint32_t height) override;
    void EndOffscreen() override;
    void Present() override;

private:
    Graphics::GraphicsDevice& target;
    const TelevisionZoom* zoom = nullptr;
};

}
