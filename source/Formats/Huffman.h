#pragma once

#include "Formats/Bytes.h"

namespace Formats
{

// Decompresses data in the Huffman format of the Nintendo BIOS routines (compression type 2). The data starts with
// a header of four bytes that holds the size of the symbols in bits (4 or 8) and the size of the result.
Bytes DecompressHuffman(ByteView compressedBytes);

}
