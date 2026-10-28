#include "Input/PointerSpace.h"

#include <cmath>

namespace Input
{

namespace
{

constexpr float PointerSpaceMinimum = -1;
constexpr float PointerSpaceMaximum = 1;
constexpr float PointerSpaceSize = PointerSpaceMaximum - PointerSpaceMinimum;

constexpr double BarsAroundThePicture = 2;

struct ShapeProportions
{
    double width;
    double height;
};

constexpr ShapeProportions FourByThreeProportions = {4, 3};
constexpr ShapeProportions SixteenByNineProportions = {16, 9};

ShapeProportions ProportionsOf(ScreenShape screenShape)
{
    if (screenShape == ScreenShape::FourByThree)
        return FourByThreeProportions;

    return SixteenByNineProportions;
}

// The space that is left in the window is shared by two bars. The first bar gets the whole pixels, the second one the rest.
double SizeOfFirstBar(double windowLength, double pictureLength)
{
    double spaceLeft = windowLength - pictureLength;
    return std::floor(spaceLeft / BarsAroundThePicture);
}

}

PictureArea FitPicture(Platform::WindowSize windowSize, ScreenShape screenShape)
{
    ShapeProportions shape = ProportionsOf(screenShape);
    double windowWidth = windowSize.width;
    double windowHeight = windowSize.height;

    double pictureWidth = 0;
    double pictureHeight = 0;

    bool windowIsTooWide = windowWidth * shape.height > windowHeight * shape.width;
    if (windowIsTooWide)
    {
        pictureHeight = windowHeight;
        pictureWidth = std::round(pictureHeight * shape.width / shape.height);
    }
    else
    {
        pictureWidth = windowWidth;
        pictureHeight = std::round(pictureWidth * shape.height / shape.width);
    }

    PictureArea pictureArea;
    pictureArea.left = static_cast<float>(SizeOfFirstBar(windowWidth, pictureWidth));
    pictureArea.top = static_cast<float>(SizeOfFirstBar(windowHeight, pictureHeight));
    pictureArea.width = static_cast<float>(pictureWidth);
    pictureArea.height = static_cast<float>(pictureHeight);
    return pictureArea;
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
