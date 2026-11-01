#pragma once

#include "Graphics/GraphicsDevice.h"

#include "Formats/Vectors.h"
#include "Platform/PixelRectangle.h"

namespace Graphics
{

// Connects the canvas units of the quads to the pixels of the area the picture covers.
// The canvas is stretched over the whole picture area, so its units can have another height than width in pixels.
// The target can be larger than the picture area, for a 16:9 picture in the middle of a screen of another shape: its whole area is then
// the plane of the canvas continued to the edges of the target. The picture area must not be empty and must lie in the middle of the
// whole area.
class CanvasMapping
{
public:
    // A target that the picture area covers completely.
    CanvasMapping(const Canvas& mappedCanvas, const Platform::PixelRectangle& coveredArea);
    CanvasMapping(const Canvas& mappedCanvas, const Platform::PixelRectangle& coveredArea, const Platform::PixelRectangle& wholeTargetArea);

    const Canvas& GetCanvas() const;
    const Platform::PixelRectangle& PictureArea() const;
    const Platform::PixelRectangle& WholeArea() const;

    // The whole area of the target in canvas units. It is the canvas when the picture area covers the whole target.
    ScissorRectangle WholeCanvasArea() const;

    Formats::Vector2 CanvasToPixel(Formats::Vector2 canvasPoint) const;
    Formats::Vector2 PixelToCanvas(Formats::Vector2 pixel) const;

    // The pixels the scissor covers. A part of a pixel counts as the whole pixel, and nothing outside the whole area is covered.
    // A scissor that covers nothing gives a rectangle without size.
    Platform::PixelRectangle ScissorToPixels(const ScissorRectangle& scissor) const;

    // The pixels of the canvas itself, which is what is drawn without a scissor.
    Platform::PixelRectangle CanvasPixels() const;

private:
    Canvas canvas;
    Platform::PixelRectangle pictureArea;
    Platform::PixelRectangle wholeArea;
};

}
