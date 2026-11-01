#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The word "Wii" as the logo of Wii Options and Wii Settings shows it, moved by the offset and scaled: the point (x, y) of the logo's
// 72 by 33 picture lands at offset + (x, y) * scale.
void AddWiiWord(VectorPath& path, Point offset, float scale);

// The white word "Wii" at the top right of Wii Options and Wii Settings (72 by 33).
void DrawWiiLogoOfTheSettings(VectorCanvas& canvas);

}
