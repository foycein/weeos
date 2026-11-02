#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The small round buttons of the Message Board (64 by 64): a gray ball lit from the upper left with a calendar on it, or with a sheet of
// lined paper and a pencil.
void DrawCalendarButton(VectorCanvas& canvas);
void DrawMemoButton(VectorCanvas& canvas);

// The buttons that scroll the Message Board (80 by 80): the cross of the + Control Pad on a bright ball inside a blue ring, its lower or
// its upper arm lit blue.
void DrawPadButtonLowerArmLit(VectorCanvas& canvas);
void DrawPadButtonUpperArmLit(VectorCanvas& canvas);

}
