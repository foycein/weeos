#include "SystemMenu/ZoomedWindow.h"

namespace SystemMenu
{

namespace
{

constexpr Formats::Rgba8Color Black = {0, 0, 0, 255};
constexpr Formats::Rgba8Color White = {255, 255, 255, 255};

// The picture of the window has two pixels for every layout unit.
constexpr float PixelsPerLayoutUnit = 2;

std::uint32_t PixelsOf(float layoutUnits)
{
    return static_cast<std::uint32_t>(layoutUnits * PixelsPerLayoutUnit);
}

// The original copies the picture without alpha (RGB565), so the picture is opaque and only the fade makes it see-through:
// the colors of the picture with the alpha of the corners of the quad.
Graphics::Material MakeOpaquePictureMaterial(Graphics::TextureHandle picture)
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.color.inputD = Graphics::TevColorInput::TextureColor;
    stage.alpha.inputD = Graphics::TevAlphaInput::RasterAlpha;

    Graphics::Material material;
    material.textures.push_back({picture, Graphics::TextureMatrix()});
    material.stages.push_back(stage);
    return material;
}

void DrawPictureInArea(Display& display, Graphics::TextureHandle picture, const PictureArea& area, std::uint8_t alpha)
{
    Formats::Rgba8Color color = White;
    color.alpha = alpha;

    Graphics::Quad quad = {};
    quad.positions = {{{area.left, area.top}, {area.right, area.top}, {area.right, area.bottom}, {area.left, area.bottom}}};
    quad.colors = {{color, color, color, color}};
    quad.textureCoordinates[0] = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    display.Device().DrawQuad(quad, MakeOpaquePictureMaterial(picture));
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

ZoomedWindow::ZoomedWindow(Display& menuDisplay)
    : display(menuDisplay)
{
}

ZoomedWindow::~ZoomedWindow()
{
    ForgetPicture();
}

// The picture starts transparent black, so what the window leaves out is black, as in the picture that the original copies.
void ZoomedWindow::DrawPicture(const std::function<void()>& drawWindow)
{
    ForgetPicture();

    Graphics::GraphicsDevice& device = display.Device();
    Graphics::Canvas canvas = display.GetCanvas();
    picture = device.BeginOffscreen(canvas, PixelsOf(canvas.width), PixelsOf(canvas.height));
    drawWindow();
    device.EndOffscreen();
}

void ZoomedWindow::ShowInsideTheTelevision(const TelevisionZoom& zoom)
{
    if (!picture.IsValid())
        return;

    PictureArea television = zoom.TelevisionInPicture();
    DrawPictureInArea(display, picture, television, zoom.WindowAlpha());
    DrawBlackAround(display, television, zoom.WindowAlpha());
    ForgetPicture();
}

void ZoomedWindow::ForgetPicture()
{
    if (!picture.IsValid())
        return;

    display.Device().DestroyTexture(picture);
    picture = Graphics::TextureHandle();
}

}
