#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/TextureFormat.h"

#include <cstdint>
#include <optional>

namespace Formats
{

// Turns the stored pixels of a texture into RGBA pixels. The result has the colors the Wii graphics chip gives:
// the intensity formats put the intensity into all four channels and the intensity and alpha formats
// put it into red, green and blue. A palette is needed for the palette formats.
RgbaImage DecodeTexture(TextureFormat format, std::uint32_t width, std::uint32_t height, ByteView encodedPixels,
    const std::optional<TexturePalette>& palette);

}
