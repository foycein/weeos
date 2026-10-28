#include "SystemMenu/Display.h"

namespace SystemMenu
{

namespace
{

constexpr float Half = 0.5f;

}

Display::Display(Graphics::GraphicsDevice& graphicsDevice, Graphics::ScreenShape initialScreenShape)
    : device(graphicsDevice)
    , screenShape(initialScreenShape)
{
}

Graphics::GraphicsDevice& Display::Device()
{
    return device;
}

Graphics::ScreenShape Display::GetScreenShape() const
{
    return screenShape;
}

void Display::SetScreenShape(Graphics::ScreenShape newScreenShape)
{
    screenShape = newScreenShape;
}

Graphics::Canvas Display::GetCanvas() const
{
    if (screenShape == Graphics::ScreenShape::SixteenByNine)
        return WideCanvas;

    return StandardCanvas;
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
