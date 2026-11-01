#include "SystemMenu/ZoomedWindow.h"

namespace SystemMenu
{

namespace
{

// The picture of the window has two pixels for every layout unit.
constexpr float PixelsPerLayoutUnit = 2;
constexpr Formats::Rgba8Color White = {255, 255, 255, 255};
constexpr Formats::Rgba8Color Black = {0, 0, 0, 255};

std::uint32_t PixelsOf(float layoutUnits)
{
    return static_cast<std::uint32_t>(layoutUnits * PixelsPerLayoutUnit);
}

void DrawPictureInArea(Graphics::GraphicsDevice& device, Graphics::TextureHandle picture, const PictureArea& area, std::uint8_t alpha)
{
    Formats::Rgba8Color color = White;
    color.alpha = alpha;

    Graphics::Quad quad = {};
    quad.positions = {{{area.left, area.top}, {area.right, area.top}, {area.right, area.bottom}, {area.left, area.bottom}}};
    quad.colors = {{color, color, color, color}};
    quad.textureCoordinates[0] = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};

    Graphics::Material material;
    material.textures.push_back({picture, Graphics::TextureMatrix()});
    device.DrawQuad(quad, material);
}

void DrawBlackAround(Display& display, const PictureArea& television, std::uint8_t alpha)
{
    Formats::Rgba8Color black = Black;
    black.alpha = alpha;

    PictureArea whole = display.WholePicture();
    display.FillArea({whole.left, television.left, whole.bottom, whole.top}, black);
    display.FillArea({television.right, whole.right, whole.bottom, whole.top}, black);
    display.FillArea({television.left, television.right, television.top, whole.top}, black);
    display.FillArea({television.left, television.right, whole.bottom, television.bottom}, black);
}

}

void DrawWindowInsideTheTelevision(Display& display, const TelevisionZoom& zoom, const std::function<void()>& drawWindow)
{
    Graphics::GraphicsDevice& device = display.Device();
    Graphics::Canvas canvas = display.GetCanvas();
    Graphics::TextureHandle picture = device.BeginOffscreen(canvas, PixelsOf(canvas.width), PixelsOf(canvas.height));
    drawWindow();
    device.EndOffscreen();

    PictureArea television = zoom.TelevisionInPicture();
    DrawPictureInArea(device, picture, television, zoom.WindowAlpha());
    device.DestroyTexture(picture);
    DrawBlackAround(display, television, zoom.WindowAlpha());
}

}
