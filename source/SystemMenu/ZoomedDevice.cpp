#include "SystemMenu/ZoomedDevice.h"

namespace SystemMenu
{

ZoomedDevice::ZoomedDevice(Graphics::GraphicsDevice& targetDevice)
    : target(targetDevice)
{
}

namespace
{

constexpr int FullAlpha = 255;

}

void ZoomedDevice::Show(const PictureArea& seenArea, const PictureArea& shownArea, std::uint8_t shownAlpha)
{
    seen = seenArea;
    shown = shownArea;
    alpha = shownAlpha;
    isMoving = true;
}

void ZoomedDevice::StopMoving()
{
    isMoving = false;
}

Formats::Vector2 ZoomedDevice::Moved(Formats::Vector2 point) const
{
    float scaleX = (shown.right - shown.left) / (seen.right - seen.left);
    float scaleY = (shown.top - shown.bottom) / (seen.top - seen.bottom);
    return {shown.left + (point.x - seen.left) * scaleX, shown.bottom + (point.y - seen.bottom) * scaleY};
}

Graphics::TextureHandle ZoomedDevice::CreateTexture(const Graphics::TextureDescription& description)
{
    return target.CreateTexture(description);
}

void ZoomedDevice::DestroyTexture(Graphics::TextureHandle texture)
{
    target.DestroyTexture(texture);
}

void ZoomedDevice::BeginFrame(const Graphics::Canvas& canvas)
{
    target.BeginFrame(canvas);
}

Graphics::ScissorRectangle ZoomedDevice::WholeArea() const
{
    return target.WholeArea();
}

void ZoomedDevice::SetScissor(const Graphics::ScissorRectangle& scissor)
{
    if (!isMoving)
    {
        target.SetScissor(scissor);
        return;
    }

    Formats::Vector2 bottomLeft = Moved({scissor.left, scissor.bottom});
    Formats::Vector2 topRight = Moved({scissor.right, scissor.top});

    Graphics::ScissorRectangle zoomedScissor;
    zoomedScissor.left = bottomLeft.x;
    zoomedScissor.bottom = bottomLeft.y;
    zoomedScissor.right = topRight.x;
    zoomedScissor.top = topRight.y;
    target.SetScissor(zoomedScissor);
}

void ZoomedDevice::ClearScissor()
{
    target.ClearScissor();
}

void ZoomedDevice::DrawQuad(const Graphics::Quad& quad, const Graphics::Material& material)
{
    if (!isMoving)
    {
        target.DrawQuad(quad, material);
        return;
    }

    Graphics::Quad zoomedQuad = quad;
    for (Formats::Vector2& position : zoomedQuad.positions)
        position = Moved(position);

    for (Formats::Rgba8Color& color : zoomedQuad.colors)
        color.alpha = static_cast<std::uint8_t>(color.alpha * alpha / FullAlpha);

    target.DrawQuad(zoomedQuad, material);
}

void ZoomedDevice::DrawMesh(std::span<const Graphics::MeshVertex> triangleCorners, const Graphics::MeshPlacement& placement,
    const Graphics::Material& material)
{
    target.DrawMesh(triangleCorners, placement, material);
}

Graphics::MeshHandle ZoomedDevice::CreateMesh(std::span<const Graphics::MeshVertex> triangleCorners)
{
    return target.CreateMesh(triangleCorners);
}

void ZoomedDevice::DestroyMesh(Graphics::MeshHandle mesh)
{
    target.DestroyMesh(mesh);
}

void ZoomedDevice::DrawMesh(Graphics::MeshHandle mesh, const Graphics::MeshPlacement& placement, const Graphics::Material& material)
{
    target.DrawMesh(mesh, placement, material);
}

Graphics::TextureHandle ZoomedDevice::BeginOffscreen(const Graphics::Canvas& canvas, std::uint32_t width, std::uint32_t height)
{
    return target.BeginOffscreen(canvas, width, height);
}

void ZoomedDevice::EndOffscreen()
{
    target.EndOffscreen();
}

void ZoomedDevice::Present()
{
    target.Present();
}

}
