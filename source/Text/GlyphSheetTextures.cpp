#include "Text/GlyphSheetTextures.h"

#include "Text/TextError.h"

#include <format>

namespace Text
{

GlyphSheetTextures::GlyphSheetTextures(Graphics::GraphicsDevice& graphicsDevice, const Formats::Font& fontToUpload)
    : device(graphicsDevice)
    , font(fontToUpload)
    , textures(fontToUpload.sheets.size())
{
}

GlyphSheetTextures::~GlyphSheetTextures()
{
    for (Graphics::TextureHandle texture : textures)
    {
        if (texture.IsValid())
            device.DestroyTexture(texture);
    }
}

Graphics::TextureHandle GlyphSheetTextures::TextureOfSheet(std::size_t sheetIndex)
{
    if (sheetIndex >= textures.size())
        throw TextError(std::format("the font has no sheet {}", sheetIndex));

    if (!textures[sheetIndex].IsValid())
        textures[sheetIndex] = UploadSheet(sheetIndex);

    return textures[sheetIndex];
}

Graphics::TextureHandle GlyphSheetTextures::UploadSheet(std::size_t sheetIndex)
{
    Formats::RgbaImage sheetImage = font.DecodeSheet(sheetIndex);

    Graphics::TextureDescription description;
    description.width = sheetImage.width;
    description.height = sheetImage.height;
    description.pixels = sheetImage.pixels;
    description.filter = Graphics::FilterMode::Linear;
    return device.CreateTexture(description);
}

}
