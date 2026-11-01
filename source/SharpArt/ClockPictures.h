#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The pictures of the clock of the Wii Menu: the ten digits (48 by 48), the colon (48 by 48) and AM and PM (48 by 32). They are
// intensity pictures, white on transparent; the menu colors them.
void DrawClockDigitZero(VectorCanvas& canvas);
void DrawClockDigitOne(VectorCanvas& canvas);
void DrawClockDigitTwo(VectorCanvas& canvas);
void DrawClockDigitThree(VectorCanvas& canvas);
void DrawClockDigitFour(VectorCanvas& canvas);
void DrawClockDigitFive(VectorCanvas& canvas);
void DrawClockDigitSix(VectorCanvas& canvas);
void DrawClockDigitSeven(VectorCanvas& canvas);
void DrawClockDigitEight(VectorCanvas& canvas);
void DrawClockDigitNine(VectorCanvas& canvas);
void DrawClockColon(VectorCanvas& canvas);
void DrawClockAm(VectorCanvas& canvas);
void DrawClockPm(VectorCanvas& canvas);

}
