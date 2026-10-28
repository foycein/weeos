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

// A picture with a red, green, blue and alpha byte for every pixel, row by row from the top left corner.
struct RgbaImage
{
    std::uint32_t width;
    std::uint32_t height;
    std::vector<Rgba8Pixel> pixels;
};

}
