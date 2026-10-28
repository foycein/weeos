#pragma once

#include "Platform/PixelRectangle.h"
#include "Platform/ScreenShape.h"
#include "Platform/WindowSize.h"

namespace Platform
{

// The largest area with the shape of the screen that fits in the window, in the middle of the window, in whole pixels.
// The rest of the window is black bars: at the sides of a window that is too wide, above and below one that is too tall.
// The graphics part draws the picture in this area and the input part measures the pointer in it.
// A window without width or height gives an empty area.
PixelRectangle FitPicture(WindowSize windowSize, ScreenShape screenShape);

}
