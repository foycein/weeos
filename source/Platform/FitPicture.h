#pragma once

#include "Platform/PixelRectangle.h"
#include "Platform/WindowSize.h"

namespace Platform
{

// The picture of WeeOS is always 16:9.
constexpr float PictureWidthShare = 16;
constexpr float PictureHeightShare = 9;

// The largest 16:9 area that fits in the window, exactly in the middle of the window. Its width and height are whole pixels; its
// left and top edge lie on half a pixel when the space beside it is an odd number of pixels.
// The graphics part draws the 16:9 picture in this area and the input part measures the pointer in it. In a window of another shape
// the space beside the area, or above and below it, is the rest of the screen, which the backgrounds of the picture fill.
// A window without width or height gives an empty area.
PixelRectangle FitPicture(WindowSize windowSize);

}
