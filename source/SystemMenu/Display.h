#pragma once

#include "Formats/Vectors.h"
#include "Graphics/GraphicsDevice.h"

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
    Display(Graphics::GraphicsDevice& graphicsDevice, Graphics::ScreenShape initialScreenShape);

    Graphics::GraphicsDevice& Device();

    Graphics::ScreenShape GetScreenShape() const;
    void SetScreenShape(Graphics::ScreenShape newScreenShape);

    // The size of the picture in layout units.
    Graphics::Canvas GetCanvas() const;

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
    Graphics::ScreenShape screenShape;
};

}
