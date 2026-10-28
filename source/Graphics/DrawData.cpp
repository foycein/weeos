#include "Graphics/DrawData.h"

#include "Formats/Bytes.h"

namespace Graphics
{

namespace
{

constexpr float FullIntensity = 255.0f;

constexpr int GreenShift = 1 * Formats::BitsPerByte;
constexpr int BlueShift = 2 * Formats::BitsPerByte;
constexpr int AlphaShift = 3 * Formats::BitsPerByte;

constexpr std::array<float, 6> IdentityTextureMatrix = {1, 0, 0, 0, 1, 0};

template <typename Enumeration>
std::uint32_t ToNumber(Enumeration enumeration)
{
    return static_cast<std::uint32_t>(enumeration);
}

ShaderColor ToShaderColor(const RegisterColor& color)
{
    return {color.red / FullIntensity, color.green / FullIntensity, color.blue / FullIntensity, color.alpha / FullIntensity};
}

ShaderColor ToShaderColor(const Formats::Rgba8Color& color)
{
    return {color.red / FullIntensity, color.green / FullIntensity, color.blue / FullIntensity, color.alpha / FullIntensity};
}

std::uint32_t PackColor(const Formats::Rgba8Color& color)
{
    std::uint32_t red = color.red;
    std::uint32_t green = color.green;
    std::uint32_t blue = color.blue;
    std::uint32_t alpha = color.alpha;
    return red | (green << GreenShift) | (blue << BlueShift) | (alpha << AlphaShift);
}

std::array<float, 6> FlattenTextureMatrix(const TextureMatrix& matrix)
{
    const auto& firstRow = matrix.rows[0];
    const auto& secondRow = matrix.rows[1];
    return {firstRow[0], firstRow[1], firstRow[2], secondRow[0], secondRow[1], secondRow[2]};
}

}

ShaderStage PackStage(const TevStage& stage)
{
    ShaderStage packed = {};

    packed.textureIndex = stage.textureIndex;
    packed.usesRasterColor = stage.usesRasterColor ? 1 : 0;
    packed.constantColor = ToNumber(stage.constantColor);
    packed.constantAlpha = ToNumber(stage.constantAlpha);

    packed.colorInputA = ToNumber(stage.color.inputA);
    packed.colorInputB = ToNumber(stage.color.inputB);
    packed.colorInputC = ToNumber(stage.color.inputC);
    packed.colorInputD = ToNumber(stage.color.inputD);
    packed.colorOperation = ToNumber(stage.color.operation);
    packed.colorBias = ToNumber(stage.color.bias);
    packed.colorScale = ToNumber(stage.color.scale);
    packed.colorClamp = stage.color.clamp ? 1 : 0;
    packed.colorOutput = ToNumber(stage.color.output);

    packed.alphaInputA = ToNumber(stage.alpha.inputA);
    packed.alphaInputB = ToNumber(stage.alpha.inputB);
    packed.alphaInputC = ToNumber(stage.alpha.inputC);
    packed.alphaInputD = ToNumber(stage.alpha.inputD);
    packed.alphaOperation = ToNumber(stage.alpha.operation);
    packed.alphaBias = ToNumber(stage.alpha.bias);
    packed.alphaScale = ToNumber(stage.alpha.scale);
    packed.alphaClamp = stage.alpha.clamp ? 1 : 0;
    packed.alphaOutput = ToNumber(stage.alpha.output);

    return packed;
}

ShaderDrawRecord MakeDrawRecord(const Quad& quad, const Material& material, std::uint32_t firstStage, std::uint32_t stageCount)
{
    ShaderDrawRecord record = {};

    for (std::size_t registerNumber = 0; registerNumber < ColorRegisterCount; ++registerNumber)
        record.colorRegisters[registerNumber] = ToShaderColor(material.colorRegisters[registerNumber]);

    for (std::size_t constantNumber = 0; constantNumber < ConstantColorCount; ++constantNumber)
        record.constantColors[constantNumber] = ToShaderColor(material.constantColors[constantNumber]);

    record.cornerPositions = quad.positions;
    record.textureCoordinates = quad.textureCoordinates;

    record.textureMatrices.fill(IdentityTextureMatrix);
    for (std::size_t textureNumber = 0; textureNumber < material.textures.size(); ++textureNumber)
        record.textureMatrices[textureNumber] = FlattenTextureMatrix(material.textures[textureNumber].matrix);

    for (std::size_t corner = 0; corner < QuadCornerCount; ++corner)
        record.cornerColors[corner] = PackColor(quad.colors[corner]);

    record.firstStage = firstStage;
    record.stageCount = stageCount;
    return record;
}

}
