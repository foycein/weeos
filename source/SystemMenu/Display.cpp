#include "SystemMenu/Display.h"

namespace SystemMenu
{

namespace
{

constexpr float Half = 0.5f;

}

Display::Display(Graphics::GraphicsDevice& graphicsDevice)
    : edgeDevice(graphicsDevice, WideCanvas)
    , zoomedDevice(edgeDevice)
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

void Display::SetTextFonts(const Layout::TextFonts& fonts)
{
    textFonts = &fonts;
}

const Layout::TextFonts* Display::TextFonts() const
{
    return textFonts;
}

Graphics::Canvas Display::GetCanvas() const
{
    return WideCanvas;
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
    edgeDevice.BeginFrame(GetCanvas());
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

void Display::NoteDrawnLayout(const Layout::LiveLayout& layout, const Layout::PlacementSettings& placement)
{
    drawnLayouts.push_back({&layout, placement});
}

void Display::ForgetDrawnLayouts()
{
    drawnLayouts.clear();
}

const std::vector<DrawnLayout>& Display::DrawnLayouts() const
{
    return drawnLayouts;
}

}
