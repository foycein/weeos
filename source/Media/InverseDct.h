#pragma once

#include "Media/JpegFrame.h"

#include <array>

namespace Media
{

// The 64 samples of one block, row by row, before the shift by 128 (from -128 to 127 for a clean picture).
using SampleBlock = std::array<float, CoefficientsPerBlock>;

// Turns the dequantized coefficients of a block back into samples (the inverse discrete cosine transform of section A.3.3
// of the JPEG standard). The two dimensions are done one after the other: first every row, then every column.
SampleBlock InverseDct(const std::array<float, CoefficientsPerBlock>& coefficients);

}
