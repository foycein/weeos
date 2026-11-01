#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

namespace Media
{

// What the frame header of a JPEG picture says, read without decoding the picture.
struct JpegInformation
{
    int width = 0;
    int height = 0;
    bool isProgressive = false;
};

// Reads a JPEG picture: baseline (SOF0), extended with 8 bit samples (SOF1) and progressive (SOF2), Huffman coded, with one
// component (gray) or three (YCbCr, or RGB), any sampling factors, restart markers. Throws a FormatError for anything else
// (arithmetic coding, 12 bit samples, lossless or hierarchical pictures, four components).
//
// The tables of Annex K are defined before the file's own: Motion-JPEG frames of AVI files leave their Huffman tables out.
// A picture whose data ends too early decodes as far as it goes.
Formats::RgbaImage DecodeJpeg(Formats::ByteView bytes);

// Reads the markers up to the frame header only.
JpegInformation ReadJpegInformation(Formats::ByteView bytes);

}
