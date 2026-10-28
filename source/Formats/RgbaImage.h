#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <vector>

namespace Formats
{

struct Rgba8Pixel
{
    Byte red;
    Byte green;
    Byte blue;
    Byte alpha;
};

// The same four bytes, where a color is meant and not a pixel of a picture.
using Rgba8Color = Rgba8Pixel;

class ByteReader;

// Reads four bytes: red, green, blue and alpha.
Rgba8Color ReadRgba8Color(ByteReader& reader);

// A picture with a red, green, blue and alpha byte for every pixel, row by row from the top left corner.
struct RgbaImage
{
    std::uint32_t width;
    std::uint32_t height;
    std::vector<Rgba8Pixel> pixels;
};

}
