#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The frame of the big buttons of Wii Options and Data Management (256 by 200): a white edge, a blue rim, a gray band at the top
// of the inside, the line above the caption and a soft shadow on the right and at the bottom.
void DrawBigOptionButtonFrame(VectorCanvas& canvas);

// The big button of Wii Settings on the page of Wii Options (256 by 200): its frame and the wrench.
void DrawWiiSettingsButton(VectorCanvas& canvas);

}
