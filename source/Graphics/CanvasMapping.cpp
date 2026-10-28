#include "Graphics/CanvasMapping.h"

#include "Graphics/GraphicsError.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Graphics
{

namespace
{

struct ShapeProportions
{
    std::uint64_t width;
    std::uint64_t height;
};

ShapeProportions ProportionsOf(ScreenShape screenShape)
{
    if (screenShape == ScreenShape::FourByThree)
        return {4, 3};

    return {16, 9};
}

}

PixelRectangle FitPicture(Platform::WindowSize windowSize, ScreenShape screenShape)
{
    ShapeProportions shape = ProportionsOf(screenShape);
    std::uint64_t windowWidth = windowSize.width;
    std::uint64_t windowHeight = windowSize.height;

    double pictureWidth = 0;
    double pictureHeight = 0;

    bool windowIsTooWide = windowWidth * shape.height > windowHeight * shape.width;
    if (windowIsTooWide)
    {
        pictureHeight = static_cast<double>(windowHeight);
        pictureWidth = std::round(pictureHeight * shape.width / shape.height);
    }
    else
    {
        pictureWidth = static_cast<double>(windowWidth);
        pictureHeight = std::round(pictureWidth * shape.height / shape.width);
    }

    double left = std::floor((static_cast<double>(windowWidth) - pictureWidth) / 2);
    double top = std::floor((static_cast<double>(windowHeight) - pictureHeight) / 2);

    return {static_cast<float>(left), static_cast<float>(top), static_cast<float>(pictureWidth), static_cast<float>(pictureHeight)};
}

CanvasMapping::CanvasMapping(const Canvas& mappedCanvas, const PixelRectangle& coveredArea)
    : canvas(mappedCanvas)
    , pictureArea(coveredArea)
{
    if (canvas.width <= 0 || canvas.height <= 0)
        throw GraphicsError("The canvas must have a width and a height");
}

const Canvas& CanvasMapping::GetCanvas() const
{
    return canvas;
}

const PixelRectangle& CanvasMapping::PictureArea() const
{
    return pictureArea;
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

PixelRectangle CanvasMapping::ScissorToPixels(const ScissorRectangle& scissor) const
{
    Formats::Vector2 topLeft = CanvasToPixel({scissor.left, scissor.top});
    Formats::Vector2 bottomRight = CanvasToPixel({scissor.right, scissor.bottom});

    float left = std::max(std::floor(topLeft.x), pictureArea.left);
    float top = std::max(std::floor(topLeft.y), pictureArea.top);
    float right = std::min(std::ceil(bottomRight.x), pictureArea.left + pictureArea.width);
    float bottom = std::min(std::ceil(bottomRight.y), pictureArea.top + pictureArea.height);

    if (right <= left || bottom <= top)
        return {pictureArea.left, pictureArea.top, 0, 0};

    return {left, top, right - left, bottom - top};
}

}
