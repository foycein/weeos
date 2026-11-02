#pragma once

#include "Graphics/GraphicsDevice.h"

#include <optional>
#include <vector>

namespace SystemMenu
{

// How a 16:9 picture fills a screen of another shape: every screen keeps its 16:9 layout in the middle,
// and the backgrounds go on to the edges of the screen, so that there are no black bars and nothing is stretched.
//
// A background is a quad that reaches across the whole canvas: from its left edge to its right edge, or from the top to the bottom.
// Such a quad goes on beyond every edge of the canvas that it reaches: a sky or a fill of the whole picture on every side, the bottom
// bar of the Wii Menu (across the whole width, down to the bottom) to the sides and down. Nothing else is drawn beyond the canvas.
//
// The quad itself stays as it is. The parts beyond the canvas are quads of their own that continue it: the texture goes on as its wrap
// mode says (the edge pixels again, or the pattern again), and the colors of the corners stay the colors of the edge they continue.
// Only quads whose sides run along the edges of the picture are continued; a turned quad is never a background.
// True for a background: an upright quad that reaches across the whole width or the whole height of the canvas.
bool IsBackground(const Graphics::Quad& quad, const Graphics::ScissorRectangle& canvasArea);

std::vector<Graphics::Quad> MakeEdgeExtensions(const Graphics::Quad& quad, const Graphics::ScissorRectangle& canvasArea,
    const Graphics::ScissorRectangle& wholeArea);

// The scissor that lets the extensions through: every edge of the scissor that lies on an edge of the canvas is moved out to the edge
// of the whole area.
Graphics::ScissorRectangle ExtendScissor(const Graphics::ScissorRectangle& scissor, const Graphics::ScissorRectangle& canvasArea,
    const Graphics::ScissorRectangle& wholeArea);

// A graphics device that draws through another one and continues the backgrounds beyond the canvas (MakeEdgeExtensions). The menu and
// the channels draw through it (SystemMenu::Display), so this is the one place that decides what reaches the edges of the screen.
// Offscreen pictures have no space beyond their canvas, so nothing is added there.
class EdgeExtendingDevice : public Graphics::GraphicsDevice
{
public:
    // The target must outlive this device. The canvas is the one of the pictures drawn through it, until BeginFrame gives another.
    EdgeExtendingDevice(Graphics::GraphicsDevice& targetDevice, const Graphics::Canvas& startingCanvas);

    Graphics::TextureHandle CreateTexture(const Graphics::TextureDescription& description) override;
    void DestroyTexture(Graphics::TextureHandle texture) override;
    void BeginFrame(const Graphics::Canvas& canvas) override;
    Graphics::ScissorRectangle WholeArea() const override;
    float ScreenPixelsPerUnit() const override;
    void SetScissor(const Graphics::ScissorRectangle& scissor) override;
    void ClearScissor() override;
    void DrawQuad(const Graphics::Quad& quad, const Graphics::Material& material) override;
    void DrawMesh(std::span<const Graphics::MeshVertex> triangleCorners, const Graphics::MeshPlacement& placement,
        const Graphics::Material& material) override;
    Graphics::MeshHandle CreateMesh(std::span<const Graphics::MeshVertex> triangleCorners) override;
    void DestroyMesh(Graphics::MeshHandle mesh) override;
    void DrawMesh(Graphics::MeshHandle mesh, const Graphics::MeshPlacement& placement, const Graphics::Material& material) override;
    Graphics::TextureHandle BeginOffscreen(const Graphics::Canvas& canvas, std::uint32_t width, std::uint32_t height) override;
    void EndOffscreen() override;
    void Present() override;

private:
    Graphics::GraphicsDevice& target;
    Graphics::ScissorRectangle canvasArea;
    std::optional<Graphics::ScissorRectangle> scissor;

    // The scissor of the screen picture while an offscreen picture is drawn, which has none of its own at the start.
    std::optional<Graphics::ScissorRectangle> screenScissor;
    Graphics::ScissorRectangle screenCanvasArea;
};

}
