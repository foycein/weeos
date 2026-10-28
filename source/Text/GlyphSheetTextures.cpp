#include "Text/GlyphSheetTextures.h"

#include "Text/TextError.h"

#include <format>

namespace Text
{

namespace
{

// The one stage that draws a glyph: the color is the color of the quad and the alpha is the alpha of the sheet times the alpha of the quad.
Graphics::TevStage MakeGlyphStage()
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.usesRasterColor = true;

    stage.color.inputD = Graphics::TevColorInput::RasterColor;

    stage.alpha.inputB = Graphics::TevAlphaInput::TextureAlpha;
    stage.alpha.inputC = Graphics::TevAlphaInput::RasterAlpha;
    return stage;
}

}

GlyphSheetTextures::GlyphSheetTextures(Graphics::GraphicsDevice& graphicsDevice, const Formats::Font& fontToUpload)
    : device(graphicsDevice)
    , font(fontToUpload)
    , materials(fontToUpload.sheets.size())
{
}

GlyphSheetTextures::~GlyphSheetTextures()
{
    for (const std::optional<Graphics::Material>& material : materials)
    {
        if (material)
            device.DestroyTexture(material->textures.front().texture);
    }
}

const Graphics::Material& GlyphSheetTextures::MaterialOfSheet(std::size_t sheetIndex)
{
    if (sheetIndex >= materials.size())
        throw TextError(std::format("the font has no sheet {}", sheetIndex));

    std::optional<Graphics::Material>& material = materials[sheetIndex];
    if (!material)
        material = MakeMaterial(sheetIndex);

    return *material;
}

Graphics::Material GlyphSheetTextures::MakeMaterial(std::size_t sheetIndex)
{
    Formats::RgbaImage sheetImage = font.DecodeSheet(sheetIndex);

    Graphics::TextureDescription description;
    description.width = sheetImage.width;
    description.height = sheetImage.height;
    description.pixels = sheetImage.pixels;
    description.filter = Graphics::FilterMode::Linear;

    Graphics::MaterialTexture sheetTexture;
    sheetTexture.texture = device.CreateTexture(description);

    Graphics::Material material;
    material.textures.push_back(sheetTexture);
    material.stages.push_back(MakeGlyphStage());
    material.blendMode = Graphics::BlendMode::StandardAlpha;
    return material;
}

}
