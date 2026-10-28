#include "Text/TextDrawer.h"

#include <map>
#include <utility>

namespace Text
{

namespace
{

constexpr Formats::Rgba8Color TransparentBlack = {0, 0, 0, 0};
constexpr Formats::Rgba8Color OpaqueWhite = {255, 255, 255, 255};

bool IsSameColor(const Formats::Rgba8Color& first, const Formats::Rgba8Color& second)
{
    return first.red == second.red && first.green == second.green && first.blue == second.blue && first.alpha == second.alpha;
}

Graphics::RegisterColor ToRegisterColor(const Formats::Rgba8Color& color)
{
    return {color.red, color.green, color.blue, color.alpha};
}

// A glyph with the usual blank and ink colors is drawn in the color of the quad.
// The alpha is the alpha of the sheet times the alpha of the quad.
std::vector<Graphics::TevStage> MakeStagesForQuadColors()
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.usesRasterColor = true;

    stage.color.inputD = Graphics::TevColorInput::RasterColor;

    stage.alpha.inputB = Graphics::TevAlphaInput::TextureAlpha;
    stage.alpha.inputC = Graphics::TevAlphaInput::RasterAlpha;
    return {stage};
}

// With other colors, the first stage mixes the blank color (register 0) and the ink color (register 1) by the intensity and
// the alpha of the sheet. The second stage multiplies the result with the color and the alpha of the quad.
std::vector<Graphics::TevStage> MakeStagesForMixedColors()
{
    Graphics::TevStage mixStage;
    mixStage.textureIndex = 0;
    mixStage.usesRasterColor = false;

    mixStage.color.inputA = Graphics::TevColorInput::Register0Color;
    mixStage.color.inputB = Graphics::TevColorInput::Register1Color;
    mixStage.color.inputC = Graphics::TevColorInput::TextureColor;

    mixStage.alpha.inputA = Graphics::TevAlphaInput::Register0Alpha;
    mixStage.alpha.inputB = Graphics::TevAlphaInput::Register1Alpha;
    mixStage.alpha.inputC = Graphics::TevAlphaInput::TextureAlpha;

    Graphics::TevStage tintStage;
    tintStage.usesRasterColor = true;

    tintStage.color.inputB = Graphics::TevColorInput::PreviousColor;
    tintStage.color.inputC = Graphics::TevColorInput::RasterColor;

    tintStage.alpha.inputB = Graphics::TevAlphaInput::PreviousAlpha;
    tintStage.alpha.inputC = Graphics::TevAlphaInput::RasterAlpha;
    return {mixStage, tintStage};
}

Graphics::Material MakeGlyphMaterial(Graphics::TextureHandle sheetTexture, const LaidOutText& text)
{
    Graphics::MaterialTexture materialTexture;
    materialTexture.texture = sheetTexture;

    Graphics::Material material;
    material.textures.push_back(materialTexture);
    material.blendMode = Graphics::BlendMode::StandardAlpha;

    bool usesDefaultColors = IsSameColor(text.blankColor, TransparentBlack) && IsSameColor(text.inkColor, OpaqueWhite);
    if (usesDefaultColors)
    {
        material.stages = MakeStagesForQuadColors();
        return material;
    }

    material.colorRegisters[0] = ToRegisterColor(text.blankColor);
    material.colorRegisters[1] = ToRegisterColor(text.inkColor);
    material.stages = MakeStagesForMixedColors();
    return material;
}

}

Formats::Vector2 TextTransform::Apply(Formats::Vector2 point) const
{
    Formats::Vector2 movedPoint;
    movedPoint.x = translation.x + point.x * xAxis.x + point.y * yAxis.x;
    movedPoint.y = translation.y + point.x * xAxis.y + point.y * yAxis.y;
    return movedPoint;
}

TextDrawer::TextDrawer(Graphics::GraphicsDevice& graphicsDevice, const Formats::Font& font)
    : device(graphicsDevice)
    , sheets(graphicsDevice, font)
{
}

void TextDrawer::Draw(const LaidOutText& text, const TextTransform& transform)
{
    // The text needs one material for each sheet that it uses.
    std::map<std::size_t, Graphics::Material> materials;

    for (const PlacedGlyph& glyph : text.glyphs)
    {
        auto material = materials.find(glyph.sheetIndex);
        if (material == materials.end())
        {
            Graphics::TextureHandle sheetTexture = sheets.TextureOfSheet(glyph.sheetIndex);
            material = materials.emplace(glyph.sheetIndex, MakeGlyphMaterial(sheetTexture, text)).first;
        }

        Graphics::Quad quad = glyph.quad;
        for (Formats::Vector2& position : quad.positions)
            position = transform.Apply(position);

        device.DrawQuad(quad, material->second);
    }
}

}
