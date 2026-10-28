#pragma once

#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"
#include "Layout/TextFonts.h"

namespace Layout
{

// Draws the text of a text box pane with the font that the layout names for it.
//
// The text fills the rectangle of the pane, placed as the text position of the box says, and is moved with the pane as the
// placement says. The alpha of the placement makes the colors of the text transparent. The two colors of the text box and the
// first two color registers of its material, the blank color and the ink color, are the live ones, so animations change them.
//
// A text box without text draws nothing. Throws a LayoutError when the text fonts do not have the font of the box.
void DrawTextBox(LiveLayout& layout, const PanePlacement& placement, const TextFonts& textFonts);

}
