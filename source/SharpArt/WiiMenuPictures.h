#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The envelope on the button of the Message Board (48 by 32), an intensity picture: white with the edge of the flap cut out.
void DrawEnvelope(VectorCanvas& canvas);

// The round gray buttons of the bar (80 by 80): the plain one under the envelope of the Message Board, and the Wii button with the
// word Wii.
void DrawRoundButton(VectorCanvas& canvas);
void DrawWiiButton(VectorCanvas& canvas);

// The soft white ring around a button of the bar that the hand points at (80 by 80), an intensity picture.
void DrawButtonGlowRing(VectorCanvas& canvas);

// The SD card beside the Wii button (38 by 48): a light blue card with its corner cut off, a white label with the SD mark and a blue
// band at the bottom.
void DrawSdCard(VectorCanvas& canvas);

// The blue arrows beside the channel grid and on many pages (64 by 64), pointing right and left.
void DrawArrowRight(VectorCanvas& canvas);
void DrawArrowLeft(VectorCanvas& canvas);

}
