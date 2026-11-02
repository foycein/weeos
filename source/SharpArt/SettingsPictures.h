#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The buttons of the pages of Wii Settings: a white pill with a blue rim, light gray above a thin gray line and white below it. They come
// in three pieces, the left end and the right end (32 by 64) and a middle strip (8 by 64) that the page stretches as wide as the button.
void DrawSettingsPageButtonLeftEnd(VectorCanvas& canvas);
void DrawSettingsPageButtonMiddle(VectorCanvas& canvas);
void DrawSettingsPageButtonRightEnd(VectorCanvas& canvas);

// The light that covers the inside of such a button while the hand points at it, an intensity picture in the same three pieces.
void DrawSettingsPageButtonLightLeftEnd(VectorCanvas& canvas);
void DrawSettingsPageButtonLightMiddle(VectorCanvas& canvas);
void DrawSettingsPageButtonLightRightEnd(VectorCanvas& canvas);

// The same light as a color picture without transparency, white on black, as the SD Card Menu keeps it.
void DrawSettingsPageButtonLightOnBlackLeftEnd(VectorCanvas& canvas);
void DrawSettingsPageButtonLightOnBlackMiddle(VectorCanvas& canvas);
void DrawSettingsPageButtonLightOnBlackRightEnd(VectorCanvas& canvas);

}
