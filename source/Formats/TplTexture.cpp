#include "Formats/TplTexture.h"

#include "Formats/ByteReader.h"
#include "Formats/TextureDecoder.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::uint32_t FileTag = 0x0020AF30;
constexpr std::uint32_t HighestWrapNumber = 2;
constexpr std::uint32_t HighestFilterNumber = 5;
constexpr std::size_t PaletteEntrySize = 2;
constexpr std::size_t PaletteHeaderUnusedSize = 2;

struct TextureOffsets
{
    std::uint32_t imageHeaderOffset;
    std::uint32_t paletteHeaderOffset;
};

TextureWrap ReadWrap(ByteReader& reader)
{
    std::uint32_t number = reader.ReadUint32();
    if (number > HighestWrapNumber)
        reader.Fail(std::format("the wrap mode {} is not known", number));

    return static_cast<TextureWrap>(number);
}

TextureFilter ReadFilter(ByteReader& reader)
{
    std::uint32_t number = reader.ReadUint32();
    if (number > HighestFilterNumber)
        reader.Fail(std::format("the filter {} is not known", number));

    return static_cast<TextureFilter>(number);
}

TextureFormat ReadFormat(ByteReader& reader)
{
    std::uint32_t number = reader.ReadUint32();

    std::optional<TextureFormat> format = TextureFormatFromNumber(number);
    if (!format)
        reader.Fail(std::format("the texture format {} is not known", number));

    return *format;
}

TexturePalette ReadPalette(ByteView bytes, std::uint32_t paletteHeaderOffset)
{
    ByteReader reader(bytes, "TPL file");
    reader.MoveTo(paletteHeaderOffset);

    TexturePalette palette;
    palette.entryCount = reader.ReadUint16();
    reader.Skip(PaletteHeaderUnusedSize);
    std::uint32_t formatNumber = reader.ReadUint32();
    std::uint32_t entriesOffset = reader.ReadUint32();

    std::optional<PaletteFormat> format = PaletteFormatFromNumber(formatNumber);
    if (!format)
        reader.Fail(std::format("the palette format {} is not known", formatNumber));
    palette.format = *format;

    reader.MoveTo(entriesOffset);
    ByteView entries = reader.ReadBytes(palette.entryCount * PaletteEntrySize);
    palette.entries.assign(entries.begin(), entries.end());
    return palette;
}

TplTexture ReadTexture(ByteView bytes, const TextureOffsets& offsets)
{
    ByteReader reader(bytes, "TPL file");
    reader.MoveTo(offsets.imageHeaderOffset);

    TplTexture texture;
    texture.height = reader.ReadUint16();
    texture.width = reader.ReadUint16();
    texture.format = ReadFormat(reader);
    std::uint32_t pixelDataOffset = reader.ReadUint32();
    texture.horizontalWrap = ReadWrap(reader);
    texture.verticalWrap = ReadWrap(reader);
    texture.minificationFilter = ReadFilter(reader);
    texture.magnificationFilter = ReadFilter(reader);
    texture.levelOfDetailBias = reader.ReadFloat();
    texture.edgeLevelOfDetailEnabled = reader.ReadByte() != 0;
    texture.minimumLevelOfDetail = reader.ReadByte();
    texture.maximumLevelOfDetail = reader.ReadByte();

    if (texture.width == 0 || texture.height == 0)
        reader.Fail("the image has no pixels");

    reader.MoveTo(pixelDataOffset);
    ByteView pixelData = reader.ReadBytes(EncodedImageSize(texture.format, texture.width, texture.height));
    texture.pixelData.assign(pixelData.begin(), pixelData.end());

    if (offsets.paletteHeaderOffset != 0)
        texture.palette = ReadPalette(bytes, offsets.paletteHeaderOffset);

    if (IsPaletteFormat(texture.format) && !texture.palette)
        reader.Fail("the image uses a palette format but has no palette");

    return texture;
}

}

std::vector<TplTexture> ReadTplTextures(ByteView bytes)
{
    ByteReader reader(bytes, "TPL file");

    std::uint32_t tag = reader.ReadUint32();
    if (tag != FileTag)
        reader.Fail(std::format("the tag {:#x} is not the TPL tag {:#x}", tag, FileTag));

    std::uint32_t textureCount = reader.ReadUint32();
    std::uint32_t tableOffset = reader.ReadUint32();

    reader.MoveTo(tableOffset);
    std::vector<TextureOffsets> offsetsOfTextures;
    for (std::uint32_t textureNumber = 0; textureNumber < textureCount; ++textureNumber)
    {
        TextureOffsets offsets;
        offsets.imageHeaderOffset = reader.ReadUint32();
        offsets.paletteHeaderOffset = reader.ReadUint32();
        offsetsOfTextures.push_back(offsets);
    }

    std::vector<TplTexture> textures;
    for (const TextureOffsets& offsets : offsetsOfTextures)
        textures.push_back(ReadTexture(bytes, offsets));

    return textures;
}

RgbaImage DecodeTexture(const TplTexture& texture)
{
    return DecodeTexture(texture.format, texture.width, texture.height, texture.pixelData, texture.palette);
}

}
