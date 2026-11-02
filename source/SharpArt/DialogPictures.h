#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The answer buttons of the dialog windows ("Yes", "No", "OK"), a gray pill with a white gloss in its upper half and a dark lower edge
// that the menu tints. Three pieces: the left end and the right end (32 by 80) and a middle strip (8 by 80) that the menu stretches.
void DrawAnswerButtonLeftEnd(VectorCanvas& canvas);
void DrawAnswerButtonMiddle(VectorCanvas& canvas);
void DrawAnswerButtonRightEnd(VectorCanvas& canvas);

// The dialog window: its upper left corner (16 by 32), its lower left corner (16 by 32), both with a thin black rim and a white inside
// that turns light gray in a few rows, and its middle (16 by 16, an intensity picture), thin white lines every four rows that the window repeats down its middle. The window mirrors
// and stretches them for its other corners and sides.
void DrawDialogWindowTopCorner(VectorCanvas& canvas);
void DrawDialogWindowBottomCorner(VectorCanvas& canvas);
void DrawDialogWindowMiddle(VectorCanvas& canvas);

}
