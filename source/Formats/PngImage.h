#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

namespace Formats
{

// Reads a PNG picture. Only the kind that the pictures of the Wii Settings pages are is understood: 8 bits for each of red, green,
// blue and alpha, and the rows one after the other (not interlaced). Anything else is a FormatError.
// The check numbers of the chunks are not compared.
RgbaImage ReadPngImage(ByteView bytes);

}
