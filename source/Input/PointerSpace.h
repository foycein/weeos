#pragma once

#include "Formats/Vectors.h"
#include "Platform/PixelRectangle.h"

#include <optional>

namespace Input
{

// Turns a pixel of the window into a place in the picture, the part of the window that Platform::FitPicture gives.
// This is the pointer space of the menu, the same for every window size and for both screen shapes: the middle of the picture
// is (0, 0), the left edge has x = -1, the right edge x = 1, the bottom edge y = -1 and the top edge y = 1.
// So y grows upward, like in a layout. A pixel in the black bars gives a number beyond -1 or 1.
// To get layout units, multiply by half the width and half the height of the layout.
// Nothing is returned while the picture has no area.
std::optional<Formats::Vector2> PixelToPointer(Formats::Vector2 pixel, const Platform::PixelRectangle& pictureArea);

}
