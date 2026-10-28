#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/TextureFormat.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Formats
{

enum class TextureWrap : std::uint32_t
{
    Clamp = 0,
    Repeat = 1,
    Mirror = 2,
};

enum class TextureFilter : std::uint32_t
{
    Nearest = 0,
    Linear = 1,
    NearestMipmapNearest = 2,
    LinearMipmapNearest = 3,
    NearestMipmapLinear = 4,
    LinearMipmapLinear = 5,
};

// One image of a TPL (texture palette library) file, with the settings for drawing it.
struct TplTexture
{
    std::uint32_t width;
    std::uint32_t height;
    TextureFormat format;

    TextureWrap horizontalWrap;
    TextureWrap verticalWrap;
    TextureFilter minificationFilter;
    TextureFilter magnificationFilter;
    float levelOfDetailBias;
    bool edgeLevelOfDetailEnabled;
    std::uint8_t minimumLevelOfDetail;
    std::uint8_t maximumLevelOfDetail;

    // The stored pixels of the full size image. The smaller copies for mipmaps that may follow are not kept.
    Bytes pixelData;

    // Only the palette formats have a palette.
    std::optional<TexturePalette> palette;
};

std::vector<TplTexture> ReadTplTextures(ByteView bytes);

RgbaImage DecodeTexture(const TplTexture& texture);

}
