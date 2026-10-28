#include "Input/PointerSpace.h"

namespace Input
{

namespace
{

constexpr float PointerSpaceMinimum = -1;
constexpr float PointerSpaceMaximum = 1;
constexpr float PointerSpaceSize = PointerSpaceMaximum - PointerSpaceMinimum;

}

std::optional<Formats::Vector2> PixelToPointer(Formats::Vector2 pixel, const Platform::PixelRectangle& pictureArea)
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
