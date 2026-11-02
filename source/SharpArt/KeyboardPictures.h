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

// The signs on keys, intensity pictures: the arrow of Backspace (56 by 32) and the bent arrow of Enter (56 by 48).
void DrawBackspaceSign(VectorCanvas& canvas);
void DrawEnterSign(VectorCanvas& canvas);

// The white arrow with a black edge of Shift (56 by 32).
void DrawShiftSign(VectorCanvas& canvas);

// The buttons that choose the kind of keyboard (64 by 32): a full keyboard and the ten keys of a phone.
void DrawFullKeyboardSign(VectorCanvas& canvas);
void DrawTenKeySign(VectorCanvas& canvas);

// The button that scrolls the text up (48 by 48): a dark arrow in a white box with a thick dark edge.
void DrawScrollArrowSign(VectorCanvas& canvas);

}
