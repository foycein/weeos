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

// The HOME button on the Close button of the HOME menu (56 by 56): a white disc with a navy rim and a light blue house.
void DrawHomeButtonPicture(VectorCanvas& canvas);

// The battery of a remote in the bar of the players: the case (48 by 24) and one of its four bars (16 by 16), intensity pictures.
void DrawBatteryCase(VectorCanvas& canvas);
void DrawBatteryBar(VectorCanvas& canvas);

// The minus and plus beside the volume (24 by 24) and one step of the volume (16 by 16, the left half white), intensity pictures.
void DrawMinusSign(VectorCanvas& canvas);
void DrawPlusSign(VectorCanvas& canvas);
void DrawVolumeStep(VectorCanvas& canvas);

// The white disc with a black edge that blinks on the buttons 1 and 2 of the remote while it connects (24 by 24).
void DrawBlinkingDisc(VectorCanvas& canvas);

// The corner of the black windows of the HOME menu with their thin white edge (16 by 16); the window turns it for its other corners.
void DrawWindowCorner(VectorCanvas& canvas);

}
