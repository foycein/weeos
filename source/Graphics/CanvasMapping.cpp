#include "Graphics/CanvasMapping.h"

#include "Graphics/GraphicsError.h"

#include <algorithm>
#include <cmath>

namespace Graphics
{

CanvasMapping::CanvasMapping(const Canvas& mappedCanvas, const Platform::PixelRectangle& coveredArea)
    : CanvasMapping(mappedCanvas, coveredArea, coveredArea)
{
}

CanvasMapping::CanvasMapping(
    const Canvas& mappedCanvas, const Platform::PixelRectangle& coveredArea, const Platform::PixelRectangle& wholeTargetArea)
    : canvas(mappedCanvas)
    , pictureArea(coveredArea)
    , wholeArea(wholeTargetArea)
{
    if (canvas.width <= 0 || canvas.height <= 0)
        throw GraphicsError("The canvas must have a width and a height");
}

const Canvas& CanvasMapping::GetCanvas() const
{
    return canvas;
}

const Platform::PixelRectangle& CanvasMapping::PictureArea() const
{
    return pictureArea;
}

const Platform::PixelRectangle& CanvasMapping::WholeArea() const
{
    return wholeArea;
}

ScissorRectangle CanvasMapping::WholeCanvasArea() const
{
    Formats::Vector2 topLeft = PixelToCanvas({wholeArea.left, wholeArea.top});
    Formats::Vector2 bottomRight = PixelToCanvas({wholeArea.left + wholeArea.width, wholeArea.top + wholeArea.height});
    return {topLeft.x, bottomRight.x, bottomRight.y, topLeft.y};
}

Formats::Vector2 CanvasMapping::CanvasToPixel(Formats::Vector2 canvasPoint) const
{
    float shareFromLeft = (canvasPoint.x + canvas.width / 2) / canvas.width;
    float shareFromTop = (canvas.height / 2 - canvasPoint.y) / canvas.height;

    return {pictureArea.left + shareFromLeft * pictureArea.width, pictureArea.top + shareFromTop * pictureArea.height};
}

Formats::Vector2 CanvasMapping::PixelToCanvas(Formats::Vector2 pixel) const
{
    float shareFromLeft = (pixel.x - pictureArea.left) / pictureArea.width;
    float shareFromTop = (pixel.y - pictureArea.top) / pictureArea.height;

    return {shareFromLeft * canvas.width - canvas.width / 2, canvas.height / 2 - shareFromTop * canvas.height};
}

Platform::PixelRectangle CanvasMapping::ScissorToPixels(const ScissorRectangle& scissor) const
{
    Formats::Vector2 topLeft = CanvasToPixel({scissor.left, scissor.top});
    Formats::Vector2 bottomRight = CanvasToPixel({scissor.right, scissor.bottom});

    float left = std::max(std::floor(topLeft.x), wholeArea.left);
    float top = std::max(std::floor(topLeft.y), wholeArea.top);
    float right = std::min(std::ceil(bottomRight.x), wholeArea.left + wholeArea.width);
    float bottom = std::min(std::ceil(bottomRight.y), wholeArea.top + wholeArea.height);

    if (right <= left || bottom <= top)
        return {wholeArea.left, wholeArea.top, 0, 0};

    return {left, top, right - left, bottom - top};
}

Platform::PixelRectangle CanvasMapping::CanvasPixels() const
{
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;
    return ScissorToPixels({-halfWidth, halfWidth, -halfHeight, halfHeight});
}

}
