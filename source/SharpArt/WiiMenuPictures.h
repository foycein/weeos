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

// The blue arrows beside the channel grid and on many pages (64 by 64), pointing right and left.
void DrawArrowRight(VectorCanvas& canvas);
void DrawArrowLeft(VectorCanvas& canvas);

}
