#include "Layout/MaterialBuilder.h"

#include "Layout/DefaultTevStages.h"
#include "Layout/LayoutError.h"
#include "Layout/QuadCorners.h"

namespace Layout
{

namespace
{

constexpr Formats::Byte FullAlpha = 255;
constexpr int FullAlphaDivisor = 255;
constexpr Formats::Rgba8Color OpaqueWhite = {255, 255, 255, 255};

using VertexColors = std::array<Formats::Rgba8Color, Formats::CornerCount>;

bool IsOpaqueWhite(const Formats::Rgba8Color& color)
{
    return color.red == OpaqueWhite.red && color.green == OpaqueWhite.green
        && color.blue == OpaqueWhite.blue && color.alpha == OpaqueWhite.alpha;
}

bool IsRegisterColor(const Formats::SignedColor& color, std::int16_t red, std::int16_t green, std::int16_t blue, std::int16_t alpha)
{
    return color.red == red && color.green == green && color.blue == blue && color.alpha == alpha;
}

Graphics::WrapMode ToWrapMode(Formats::TextureWrap wrap)
{
    switch (wrap)
    {
    case Formats::TextureWrap::Clamp:
        return Graphics::WrapMode::Clamp;
    case Formats::TextureWrap::Repeat:
        return Graphics::WrapMode::Repeat;
    case Formats::TextureWrap::Mirror:
        return Graphics::WrapMode::Mirror;
    }
    throw LayoutError("A texture has a wrap mode that is not known");
}

Graphics::RegisterColor ToRegisterColor(const Formats::SignedColor& color)
{
    return {color.red, color.green, color.blue, color.alpha};
}

// The alpha of the pane makes the colors transparent. A pane with the full alpha leaves them as they are.
Formats::Rgba8Color ScaleAlpha(Formats::Rgba8Color color, std::uint8_t paneAlpha)
{
    if (paneAlpha == FullAlpha)
        return color;

    color.alpha = static_cast<Formats::Byte>(color.alpha * paneAlpha / FullAlphaDivisor);
    return color;
}

// Without a channel control, the colors and the alpha come from the vertex colors.
Formats::ChannelControl ChannelControlOf(const Formats::Material& description)
{
    Formats::ChannelControl vertexColorsOnly = {Formats::ColorSource::Vertex, Formats::ColorSource::Vertex};
    return description.channelControl.value_or(vertexColorsOnly);
}

bool ReadsColorSource(const Formats::ChannelControl& channel, Formats::ColorSource source)
{
    return channel.colorSource == source || channel.alphaSource == source;
}

// The color of a corner comes from the vertex color or from the color of the material: one source for red, green and blue,
// and one source for the alpha.
Formats::Rgba8Color MakeRasterColor(const Formats::ChannelControl& channel, const Formats::Rgba8Color& vertexColor,
    const Formats::Rgba8Color& materialColor)
{
    const Formats::Rgba8Color& colorSide = channel.colorSource == Formats::ColorSource::Vertex ? vertexColor : materialColor;
    const Formats::Rgba8Color& alphaSide = channel.alphaSource == Formats::ColorSource::Vertex ? vertexColor : materialColor;
    return {colorSide.red, colorSide.green, colorSide.blue, alphaSide.alpha};
}

std::array<Formats::Rgba8Color, Graphics::QuadCornerCount> MakeCornerColors(const Formats::ChannelControl& channel,
    const Formats::Rgba8Color& materialColor, std::uint8_t paneAlpha, const VertexColors& vertexColors)
{
    VertexColors rasterColors;
    for (std::size_t corner = 0; corner < Formats::CornerCount; ++corner)
    {
        Formats::Rgba8Color vertexColor = ScaleAlpha(vertexColors[corner], paneAlpha);
        rasterColors[corner] = MakeRasterColor(channel, vertexColor, materialColor);
    }
    return InQuadOrder(rasterColors);
}

bool NeedsVertexColorMultiplication(std::uint8_t paneAlpha, const VertexColors& vertexColors)
{
    if (paneAlpha != FullAlpha)
        return true;

    for (const Formats::Rgba8Color& color : vertexColors)
    {
        if (!IsOpaqueWhite(color))
            return true;
    }
    return false;
}

// The default stages end with a multiplication by the raster color, but only when the raster color changes something.
bool NeedsRasterColorMultiplication(const Formats::ChannelControl& channel, const Formats::Rgba8Color& materialColor,
    std::uint8_t paneAlpha, const VertexColors& vertexColors)
{
    bool vertexColorsChangeTheResult = ReadsColorSource(channel, Formats::ColorSource::Vertex)
        && NeedsVertexColorMultiplication(paneAlpha, vertexColors);
    bool materialColorChangesTheResult = ReadsColorSource(channel, Formats::ColorSource::Material) && !IsOpaqueWhite(materialColor);
    return vertexColorsChangeTheResult || materialColorChangesTheResult;
}

std::vector<Graphics::TevStage> ChooseStages(const LiveMaterial& liveMaterial, const Formats::ChannelControl& channel,
    const Formats::Rgba8Color& materialColor, std::uint8_t paneAlpha, const VertexColors& vertexColors)
{
    if (!liveMaterial.stages.empty())
        return liveMaterial.stages;

    const Formats::Material& description = liveMaterial.description;
    bool tintsTextures = !IsRegisterColor(description.foreColor, 0, 0, 0, 0)
        || !IsRegisterColor(description.backColor, 255, 255, 255, 255);
    bool multipliesByRasterColor = NeedsRasterColorMultiplication(channel, materialColor, paneAlpha, vertexColors);
    return MakeDefaultTevStages(description.textures.size(), tintsTextures, multipliesByRasterColor);
}

Graphics::MaterialTexture MakeMaterialTexture(LiveLayout& layout, const LiveMaterial& liveMaterial, std::size_t textureMapIndex)
{
    Graphics::MaterialTexture texture;
    texture.texture = ObtainTextureOfMap(layout, liveMaterial, textureMapIndex).handle;
    texture.matrix = TextureMatrixOf(liveMaterial.description, textureMapIndex);
    return texture;
}

}

LoadedTexture ObtainTextureOfMap(LiveLayout& layout, const LiveMaterial& liveMaterial, std::size_t textureMapIndex)
{
    const Formats::MaterialTexture& fileTexture = liveMaterial.description.textures[textureMapIndex];
    return layout.ObtainTexture(liveMaterial.textureNames[textureMapIndex], ToWrapMode(fileTexture.horizontalWrap), ToWrapMode(fileTexture.verticalWrap));
}

DrawableMaterial BuildDrawableMaterial(LiveLayout& layout, const LiveMaterial& liveMaterial, std::uint8_t paneAlpha, const VertexColors& vertexColors)
{
    const Formats::Material& description = liveMaterial.description;
    Formats::ChannelControl channel = ChannelControlOf(description);
    Formats::Rgba8Color materialColor = ScaleAlpha(description.materialColor.value_or(OpaqueWhite), paneAlpha);

    DrawableMaterial drawable;
    drawable.cornerColors = MakeCornerColors(channel, materialColor, paneAlpha, vertexColors);

    Graphics::Material& material = drawable.material;
    for (std::size_t textureMapIndex = 0; textureMapIndex < description.textures.size(); ++textureMapIndex)
        material.textures.push_back(MakeMaterialTexture(layout, liveMaterial, textureMapIndex));

    material.colorRegisters = {ToRegisterColor(description.foreColor), ToRegisterColor(description.backColor), ToRegisterColor(description.colorRegister3)};
    material.constantColors = description.constantColors;
    material.stages = ChooseStages(liveMaterial, channel, materialColor, paneAlpha, vertexColors);
    material.blendMode = liveMaterial.blendMode;
    return drawable;
}

}
