#pragma once

#include "Graphics/GraphicsDevice.h"

#include "Formats/Vectors.h"
#include "Platform/PixelRectangle.h"

namespace Graphics
{

// Connects the canvas units of the quads to the pixels of the area the picture covers.
// The canvas is stretched over the whole picture area, so its units can have another height than width in pixels.
// The picture area must not be empty.
class CanvasMapping
{
public:
    CanvasMapping(const Canvas& mappedCanvas, const Platform::PixelRectangle& coveredArea);

    const Canvas& GetCanvas() const;
    const Platform::PixelRectangle& PictureArea() const;

    Formats::Vector2 CanvasToPixel(Formats::Vector2 canvasPoint) const;
    Formats::Vector2 PixelToCanvas(Formats::Vector2 pixel) const;

    // The pixels the scissor covers. A part of a pixel counts as the whole pixel, and nothing outside the picture area is covered.
    // A scissor that covers nothing gives a rectangle without size.
    Platform::PixelRectangle ScissorToPixels(const ScissorRectangle& scissor) const;

private:
    Canvas canvas;
    Platform::PixelRectangle pictureArea;
};

}
