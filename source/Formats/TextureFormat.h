#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace Formats
{

// The ways the Wii stores the pixels of a texture. The names in the Wii documents are written in brackets:
// I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, C4, C8, C14X2 and CMPR.
enum class TextureFormat : std::uint32_t
{
    Intensity4 = 0,
    Intensity8 = 1,
    IntensityAlpha4 = 2,
    IntensityAlpha8 = 3,
    Rgb565 = 4,
    Rgb5A3 = 5,
    Rgba8 = 6,
    Palette4 = 8,
    Palette8 = 9,
    Palette14 = 10,
    Compressed = 14,
};

// How the colors of a palette are stored. A palette entry has 16 bits in every format.
enum class PaletteFormat : std::uint32_t
{
    IntensityAlpha8 = 0,
    Rgb565 = 1,
    Rgb5A3 = 2,
};

struct TexturePalette
{
    PaletteFormat format;
    std::size_t entryCount;
    Bytes entries;
};

std::optional<TextureFormat> TextureFormatFromNumber(std::uint32_t number);
std::optional<PaletteFormat> PaletteFormatFromNumber(std::uint32_t number);

// A texture is stored in blocks of pixels. An image is padded to a whole number of blocks.
struct BlockShape
{
    std::uint32_t width;
    std::uint32_t height;
    std::size_t byteSize;
};

BlockShape BlockShapeOf(TextureFormat format);

// The size in bytes of the stored pixels of one image, including the padding to whole blocks.
std::size_t EncodedImageSize(TextureFormat format, std::uint32_t width, std::uint32_t height);

bool IsPaletteFormat(TextureFormat format);

}
