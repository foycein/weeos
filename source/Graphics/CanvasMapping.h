#pragma once

#include "Graphics/GraphicsDevice.h"

#include "Formats/Vectors.h"
#include "Platform/WindowSize.h"

namespace Graphics
{

// A rectangle of pixels. The origin is the top left corner of the target, x grows to the right and y grows downward.
// The rectangles that come from the functions below have whole pixel numbers.
struct PixelRectangle
{
    float left = 0;
    float top = 0;
    float width = 0;
    float height = 0;
};

// The largest area with the shape of the screen that fits in the window, in the middle of the window.
// The rest of the window is black bars: at the sides of a window that is too wide, above and below one that is too tall.
PixelRectangle FitPicture(Platform::WindowSize windowSize, ScreenShape screenShape);

// Connects the canvas units of the quads to the pixels of the area the picture covers.
// The canvas is stretched over the whole picture area, so its units can have another height than width in pixels.
// The picture area must not be empty.
class CanvasMapping
{
public:
    CanvasMapping(const Canvas& mappedCanvas, const PixelRectangle& coveredArea);

    const Canvas& GetCanvas() const;
    const PixelRectangle& PictureArea() const;

    Formats::Vector2 CanvasToPixel(Formats::Vector2 canvasPoint) const;
    Formats::Vector2 PixelToCanvas(Formats::Vector2 pixel) const;

    // The pixels the scissor covers. A part of a pixel counts as the whole pixel, and nothing outside the picture area is covered.
    // A scissor that covers nothing gives a rectangle without size.
    PixelRectangle ScissorToPixels(const ScissorRectangle& scissor) const;

private:
    Canvas canvas;
    PixelRectangle pictureArea;
};

}
