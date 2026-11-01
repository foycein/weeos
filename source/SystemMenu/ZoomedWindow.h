#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/TelevisionZoom.h"

#include <functional>

namespace SystemMenu
{

// While a list of channels zooms into a television, the window of the channel is a picture inside the television, with black around it
// (0x813B5A54 and 0x813B9BFC for the channel grid, 0x813E5F54 for the SD Card Menu). What drawWindow draws is shown where the
// television is now, as much as the zoom has come.
void DrawWindowInsideTheTelevision(Display& display, const TelevisionZoom& zoom, const std::function<void()>& drawWindow);

}
