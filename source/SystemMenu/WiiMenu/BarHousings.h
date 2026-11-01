#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/MenuLayout.h"

namespace SystemMenu
{

// Draws the layout of the bottom bar. On a screen wider than the 16:9 picture (docs/DECISIONS.md, Settings) the housings of the buttons
// that reach over a side of the picture go on to the side of the screen: their middle piece grows outward, and their outer end moves
// out with it beyond the screen. The buttons of the bar that wait beside the picture (those of the Message Board) stay hidden.
void DrawBarWithHousingsToTheSides(MenuLayout& layout, Display& display);

}
