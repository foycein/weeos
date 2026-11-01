#include "SystemMenu/ZoomedDevice.h"

namespace SystemMenu
{

ZoomedDevice::ZoomedDevice(Graphics::GraphicsDevice& targetDevice)
    : target(targetDevice)
{
}

void ZoomedDevice::LookThrough(const TelevisionZoom& televisionZoom)
{
    zoom = &televisionZoom;
}

Graphics::TextureHandle ZoomedDevice::CreateTexture(const Graphics::TextureDescription& description)
{
    return target.CreateTexture(description);
}

void ZoomedDevice::DestroyTexture(Graphics::TextureHandle texture)
{
    target.DestroyTexture(texture);
}

void ZoomedDevice::BeginFrame(const Graphics::Canvas& canvas, Platform::ScreenShape screenShape)
{
    target.BeginFrame(canvas, screenShape);
}

void ZoomedDevice::SetScissor(const Graphics::ScissorRectangle& scissor)
{
    Formats::Vector2 bottomLeft = zoom->PointInPicture({scissor.left, scissor.bottom});
    Formats::Vector2 topRight = zoom->PointInPicture({scissor.right, scissor.top});

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
    Graphics::Quad zoomedQuad = quad;
    for (Formats::Vector2& position : zoomedQuad.positions)
        position = zoom->PointInPicture(position);

    target.DrawQuad(zoomedQuad, material);
}

void ZoomedDevice::DrawMesh(std::span<const Graphics::MeshVertex> triangleCorners, const Graphics::MeshPlacement& placement,
    const Graphics::Material& material)
{
    target.DrawMesh(triangleCorners, placement, material);
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
