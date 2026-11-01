#pragma once

#include "SystemMenu/ZoomedDevice.h"

#include "Formats/Vectors.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/TextFonts.h"
#include "Platform/ScreenShape.h"

#include <optional>

namespace SystemMenu
{

// The picture of a 4:3 screen is 608 by 456 layout units. The picture of a 16:9 screen is 832 by 456 units.
constexpr Graphics::Canvas StandardCanvas = {608, 456};
constexpr Graphics::Canvas WideCanvas = {832, 456};

// The picture the menu draws into: the graphics device and the shape of the screen.
// The shape can change while the menu runs, so whoever draws asks for it again in every step.
class Display
{
public:
    Display(Graphics::GraphicsDevice& graphicsDevice, Platform::ScreenShape initialScreenShape);

    // The device to draw with. While an area is moved, the device moves what is drawn. Whatever draws for the display keeps this
    // device, also the text of the layouts, so that it is moved too.
    Graphics::GraphicsDevice& Device();

    // Everything that is drawn from now on into seenArea lands in shownArea, with its alpha scaled by alpha / 255, until StopMovingAreas.
    void MoveArea(const PictureArea& seenArea, const PictureArea& shownArea, std::uint8_t alpha);
    void StopMovingAreas();

    // The camera of the layouts (0x81089830 of the original): the area of the layouts that fills the picture. The channel grid moves it
    // into a television while it zooms. The screens of the Wii Menu look through it; the hands, the dialogs and the windows do not.
    void SetCamera(const PictureArea& visibleArea);
    void ResetCamera();

    // What is drawn from now on is seen through the camera, until StopMovingAreas.
    void LookThroughCamera();

    // The fonts that the text boxes of the layouts are drawn with. Without fonts the text boxes draw nothing.
    // The fonts must stay alive as long as the display draws.
    void SetTextFonts(const Layout::TextFonts& fonts);
    const Layout::TextFonts* TextFonts() const;

    Platform::ScreenShape GetScreenShape() const;
    void SetScreenShape(Platform::ScreenShape newScreenShape);

    // The size of the picture in layout units.
    Graphics::Canvas GetCanvas() const;

    // The whole picture, in layout units.
    PictureArea WholePicture() const;

    // Paints a rectangle of the picture in one color.
    void FillArea(const PictureArea& area, Formats::Rgba8Color color);

    // Starts a new picture of the screen. The program shows it with Present when everything is drawn.
    void BeginPicture();

    // How much a layout is stretched sideways to fill the picture. The layouts are 608 by 456 units, so it is (832 / 608, 1) on a 16:9
    // screen and (1, 1) on a 4:3 screen. Panes that adjust their location are squeezed back by the opposite, so they keep their shape.
    Formats::Vector2 LayoutStretch() const;

    // A place in the pointer space of the Input part (the middle of the picture is (0, 0), the edges are -1 and 1, y grows upward)
    // as a place in the picture, in layout units.
    Formats::Vector2 PointerToCanvas(Formats::Vector2 pointer) const;

    // A place in the picture, as it is written into a pane that sits right below the root pane of a stretched layout.
    // The root pane stretches the place, so the pane gets the place divided by the stretch.
    Formats::Vector2 CanvasToRootPaneSpace(Formats::Vector2 canvasPoint) const;

private:
    Graphics::GraphicsDevice& device;
    ZoomedDevice zoomedDevice;
    std::optional<PictureArea> camera;
    const Layout::TextFonts* textFonts = nullptr;
    Platform::ScreenShape screenShape;
};

}
