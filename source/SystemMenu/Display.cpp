#include "SystemMenu/Display.h"

namespace SystemMenu
{

namespace
{

constexpr float Half = 0.5f;
constexpr std::uint8_t FullyShown = 255;

}

Display::Display(Graphics::GraphicsDevice& graphicsDevice, Platform::ScreenShape initialScreenShape)
    : device(graphicsDevice)
    , zoomedDevice(graphicsDevice)
    , screenShape(initialScreenShape)
{
}

Graphics::GraphicsDevice& Display::Device()
{
    return zoomedDevice;
}

void Display::MoveArea(const PictureArea& seenArea, const PictureArea& shownArea, std::uint8_t alpha)
{
    zoomedDevice.Show(seenArea, shownArea, alpha);
}

void Display::StopMovingAreas()
{
    zoomedDevice.StopMoving();
}

void Display::SetCamera(const PictureArea& visibleArea)
{
    camera = visibleArea;
}

void Display::ResetCamera()
{
    camera.reset();
}

void Display::LookThroughCamera()
{
    if (camera)
        MoveArea(*camera, WholePicture(), FullyShown);
}

void Display::SetTextFonts(const Layout::TextFonts& fonts)
{
    textFonts = &fonts;
}

const Layout::TextFonts* Display::TextFonts() const
{
    return textFonts;
}

Platform::ScreenShape Display::GetScreenShape() const
{
    return screenShape;
}

void Display::SetScreenShape(Platform::ScreenShape newScreenShape)
{
    screenShape = newScreenShape;
}

Graphics::Canvas Display::GetCanvas() const
{
    if (screenShape == Platform::ScreenShape::SixteenByNine)
        return WideCanvas;

    return StandardCanvas;
}

PictureArea Display::WholePicture() const
{
    Graphics::Canvas canvas = GetCanvas();
    PictureArea area;
    area.left = -canvas.width * Half;
    area.right = canvas.width * Half;
    area.bottom = -canvas.height * Half;
    area.top = canvas.height * Half;
    return area;
}

void Display::FillArea(const PictureArea& area, Formats::Rgba8Color color)
{
    Graphics::Quad quad = {};
    quad.positions = {{{area.left, area.top}, {area.right, area.top}, {area.right, area.bottom}, {area.left, area.bottom}}};
    quad.colors = {{color, color, color, color}};
    Device().DrawQuad(quad, Graphics::Material());
}

void Display::BeginPicture()
{
    device.BeginFrame(GetCanvas(), screenShape);
}

Formats::Vector2 Display::LayoutStretch() const
{
    Graphics::Canvas canvas = GetCanvas();
    return {canvas.width / StandardCanvas.width, canvas.height / StandardCanvas.height};
}

Formats::Vector2 Display::PointerToCanvas(Formats::Vector2 pointer) const
{
    Graphics::Canvas canvas = GetCanvas();
    return {pointer.x * canvas.width * Half, pointer.y * canvas.height * Half};
}

Formats::Vector2 Display::CanvasToRootPaneSpace(Formats::Vector2 canvasPoint) const
{
    Formats::Vector2 stretch = LayoutStretch();
    return {canvasPoint.x / stretch.x, canvasPoint.y / stretch.y};
}

}
