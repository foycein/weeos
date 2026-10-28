#pragma once

#include "Formats/Bytes.h"

namespace Formats
{

// True when the data starts with the tag "ASH0" of the compression the System Menu uses for its layout archives.
bool IsAsh0Compressed(ByteView bytes);

Bytes DecompressAsh0(ByteView compressedBytes);

}
