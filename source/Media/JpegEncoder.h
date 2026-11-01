#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

namespace Media
{

// Writes a baseline JPEG file (JFIF): YCbCr with the colors at half the size in both directions (4:2:0), the quantization tables of
// Annex K scaled to the quality (1 to 100, as the IJG library counts it), and the Huffman tables of Annex K. The alpha is not kept.
Formats::Bytes EncodeJpeg(const Formats::RgbaImage& picture, int quality);

}
