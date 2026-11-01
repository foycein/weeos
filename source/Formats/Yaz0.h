#pragma once

#include "Formats/Bytes.h"

namespace Formats
{

// True when the data starts with the tag of Yaz0 compression ("Yaz0").
bool IsYaz0Compressed(ByteView bytes);

// Unpacks Yaz0 data (the .szs files of the channels). Throws a FormatError when the data is not Yaz0 or ends too early.
Bytes DecompressYaz0(ByteView compressed);

}
