#pragma once

#include "Media/JpegFrame.h"

#include "Formats/RgbaImage.h"

#include <array>

namespace Media
{

constexpr int QuantizationTableSlots = 4;

// How the components of a picture with three of them are colors.
enum class JpegColors
{
    // Brightness and two color differences (YCbCr), the usual case (JFIF).
    BrightnessAndColorDifferences,

    // Red, green and blue as they are (an Adobe picture with transform 0, or components named R, G and B).
    RedGreenBlue,
};

// Makes the picture from the decoded coefficients: dequantizes and transforms every block back into samples, brings the
// components that have fewer samples up to the size of the picture, and turns the samples into red, green and blue.
Formats::RgbaImage MakeJpegPicture(const JpegFrame& frame, const std::array<QuantizationTable, QuantizationTableSlots>& quantizationTables,
    JpegColors colors);

}
