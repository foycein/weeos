#include "Formats/TextureFormat.h"

#include "Formats/Alignment.h"
#include "Formats/FormatError.h"

#include <array>
#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t SmallBlockByteSize = 32;
constexpr std::size_t LargeBlockByteSize = 64;

constexpr std::array<TextureFormat, 11> AllTextureFormats = {
    TextureFormat::Intensity4,
    TextureFormat::Intensity8,
    TextureFormat::IntensityAlpha4,
    TextureFormat::IntensityAlpha8,
    TextureFormat::Rgb565,
    TextureFormat::Rgb5A3,
    TextureFormat::Rgba8,
    TextureFormat::Palette4,
    TextureFormat::Palette8,
    TextureFormat::Palette14,
    TextureFormat::Compressed,
};

constexpr std::array<PaletteFormat, 3> AllPaletteFormats = {
    PaletteFormat::IntensityAlpha8,
    PaletteFormat::Rgb565,
    PaletteFormat::Rgb5A3,
};

}

std::optional<TextureFormat> TextureFormatFromNumber(std::uint32_t number)
{
    for (TextureFormat format : AllTextureFormats)
    {
        if (static_cast<std::uint32_t>(format) == number)
            return format;
    }

    return std::nullopt;
}

std::optional<PaletteFormat> PaletteFormatFromNumber(std::uint32_t number)
{
    for (PaletteFormat format : AllPaletteFormats)
    {
        if (static_cast<std::uint32_t>(format) == number)
            return format;
    }

    return std::nullopt;
}

BlockShape BlockShapeOf(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::Intensity4:
    case TextureFormat::Palette4:
    case TextureFormat::Compressed:
        return {8, 8, SmallBlockByteSize};

    case TextureFormat::Intensity8:
    case TextureFormat::IntensityAlpha4:
    case TextureFormat::Palette8:
        return {8, 4, SmallBlockByteSize};

    case TextureFormat::IntensityAlpha8:
    case TextureFormat::Rgb565:
    case TextureFormat::Rgb5A3:
    case TextureFormat::Palette14:
        return {4, 4, SmallBlockByteSize};

    case TextureFormat::Rgba8:
        return {4, 4, LargeBlockByteSize};
    }

    throw FormatError(std::format("The texture format {} is not known", static_cast<std::uint32_t>(format)));
}

std::size_t EncodedImageSize(TextureFormat format, std::uint32_t width, std::uint32_t height)
{
    BlockShape shape = BlockShapeOf(format);
    std::size_t blocksAcross = RoundUpToMultiple(width, shape.width) / shape.width;
    std::size_t blocksDown = RoundUpToMultiple(height, shape.height) / shape.height;
    return blocksAcross * blocksDown * shape.byteSize;
}

bool IsPaletteFormat(TextureFormat format)
{
    return format == TextureFormat::Palette4 || format == TextureFormat::Palette8 || format == TextureFormat::Palette14;
}

}
