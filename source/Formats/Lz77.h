#pragma once

#include "Formats/Bytes.h"

namespace Formats
{

// True when the data starts with the tag "LZ77". Some files have the compressed data without the tag.
bool HasLz77Tag(ByteView bytes);

// Decompresses LZ77 data of type 0x10, with or without the tag "LZ77" in front.
Bytes DecompressLz77(ByteView compressedBytes);

}
