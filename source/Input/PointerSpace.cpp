#include "Input/PointerSpace.h"

#include <cmath>

namespace Input
{

namespace
{

constexpr float PointerSpaceMinimum = -1;
constexpr float PointerSpaceMaximum = 1;
constexpr float PointerSpaceSize = PointerSpaceMaximum - PointerSpaceMinimum;

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

PictureArea FitPicture(WindowSize windowSize, ScreenShape screenShape)
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

std::optional<Formats::Vector2> PixelToPointer(Formats::Vector2 pixel, const PictureArea& pictureArea)
{
    if (pictureArea.IsEmpty())
        return std::nullopt;

    float shareFromLeft = (pixel.x - pictureArea.left) / pictureArea.width;
    float shareFromTop = (pixel.y - pictureArea.top) / pictureArea.height;

    Formats::Vector2 pointer;
    pointer.x = PointerSpaceMinimum + shareFromLeft * PointerSpaceSize;
    pointer.y = PointerSpaceMaximum - shareFromTop * PointerSpaceSize;
    return pointer;
}

}
