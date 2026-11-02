#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The glass keys of the soft keyboard: a gray key with a thin black edge, a light gloss along its top and a gray that lightens downward.
// A letter key (42 by 34), a wide key (72 by 34), the space bar (156 by 34) and the tall Enter key (70 by 66).
void DrawLetterKey(VectorCanvas& canvas);
void DrawWideKey(VectorCanvas& canvas);
void DrawSpaceKey(VectorCanvas& canvas);
void DrawTallKey(VectorCanvas& canvas);

// The white plate with a thin black edge behind a key that is pressed (32 by 32).
void DrawRoundPlate(VectorCanvas& canvas);

}
