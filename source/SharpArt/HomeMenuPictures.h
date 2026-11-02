#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The big button of the HOME menu ("Wii Menu", "Reset"), a glossy gray pill lit from the upper left that the menu tints. It comes in three
// pieces: the left end and the right end (56 by 112) and a middle strip (8 by 112) that the menu stretches as wide as the button needs.
void DrawBigButtonLeftEnd(VectorCanvas& canvas);
void DrawBigButtonMiddle(VectorCanvas& canvas);
void DrawBigButtonRightEnd(VectorCanvas& canvas);

// The buttons of the remote settings (Rumble On and Off, Reconnect, the volume), a gray rounded box with a white gloss in the upper left
// and a darker right side and bottom. The same three pieces: the left end and the right end (24 by 64) and a middle strip (8 by 64).
void DrawSettingsButtonLeftEnd(VectorCanvas& canvas);
void DrawSettingsButtonMiddle(VectorCanvas& canvas);
void DrawSettingsButtonRightEnd(VectorCanvas& canvas);

}
