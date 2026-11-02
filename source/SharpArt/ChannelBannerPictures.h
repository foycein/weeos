#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The outline of the word Mii on the Mii Channel's icon (72 by 48), an intensity picture that cuts the logo out: the M with its two
// round tops, and the two i with their round dots.
void DrawMiiLogoShape(VectorCanvas& canvas);

// The colors of the word Mii under that outline (72 by 48): a gray-green ground and the letters a little inside it, white turning light
// green towards the bottom, with smiling faces on the dots of the i.
void DrawMiiLogoColors(VectorCanvas& canvas);

// The frame behind the photographs of the Photo Channel's banner (208 by 148): a light gray card with a soft shadow to the right and below.
void DrawPhotoFrame(VectorCanvas& canvas);

}
