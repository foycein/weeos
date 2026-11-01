#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

namespace Formats
{

// Reads the first picture of a GIF file, with the size of that picture. A pixel that has the transparent color of the file has the alpha 0.
// Later pictures of an animation are ignored. A picture with interlaced rows is a FormatError.
RgbaImage ReadGifImage(ByteView bytes);

}
