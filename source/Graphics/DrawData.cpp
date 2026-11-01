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

std::uint32_t PackAlphaTest(const std::optional<AlphaTest>& test)
{
    constexpr int SecondComparisonShift = 3;
    constexpr int JoinShift = 6;
    constexpr int FirstReferenceShift = 8;
    constexpr int SecondReferenceShift = 16;
    constexpr std::uint32_t TestIsOn = 0x80000000;

    if (!test)
        return 0;

    std::uint32_t packed = TestIsOn;
    packed |= ToNumber(test->firstComparison);
    packed |= ToNumber(test->secondComparison) << SecondComparisonShift;
    packed |= ToNumber(test->join) << JoinShift;
    packed |= static_cast<std::uint32_t>(test->firstReference) << FirstReferenceShift;
    packed |= static_cast<std::uint32_t>(test->secondReference) << SecondReferenceShift;
    return packed;
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

    constexpr int BitsPerChannel = 2;
    constexpr int TextureSwapShift = 8;
    for (std::size_t channel = 0; channel < stage.rasterSwap.size(); ++channel)
    {
        int shift = static_cast<int>(channel) * BitsPerChannel;
        packed.swaps |= std::uint32_t{stage.rasterSwap[channel]} << shift;
        packed.swaps |= std::uint32_t{stage.textureSwap[channel]} << (TextureSwapShift + shift);
    }
    packed.isMovedByIndirectTexture = stage.isMovedByIndirectTexture ? 1 : 0;

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
    record.skipsTransparentPixels = material.skipsTransparentPixels ? 1 : 0;
    record.alphaTest = PackAlphaTest(material.alphaTest);

    if (material.indirectTexture)
    {
        record.indirectTextureIndex = material.indirectTexture->textureIndex;
        const auto& rows = material.indirectTexture->matrix;
        record.indirectMatrix = {rows[0][0], rows[0][1], rows[0][2], rows[1][0], rows[1][1], rows[1][2]};
    }
    return record;
}

ShaderMeshRecord MakeMeshRecord(const MeshPlacement& placement)
{
    ShaderMeshRecord record = {};

    Matrix4 normalMatrix = MakeNormalMatrix(placement.modelView);
    for (std::size_t row = 0; row < record.modelView.size(); ++row)
    {
        record.modelView[row] = placement.modelView.rows[row];
        record.normalMatrix[row] = normalMatrix.rows[row];
    }
    record.projection = placement.projection.rows;

    if (placement.light)
    {
        const Formats::Vector3& position = placement.light->position;
        record.lightPosition = {position.x, position.y, position.z, 1};
        record.lightColor = ToShaderColor(placement.light->color);
        record.ambientColor = ToShaderColor(placement.light->ambientColor);
        record.hasLight = 1;
    }

    record.textureCoordinatesFromNormal = placement.textureCoordinatesFromNormal ? 1 : 0;
    record.usesCornerColors = placement.usesCornerColors ? 1 : 0;
    return record;
}

ShaderMeshCorner MakeMeshCorner(const MeshVertex& vertex)
{
    return {vertex.position, vertex.textureCoordinate.x, vertex.normal, vertex.textureCoordinate.y, vertex.boneNumber, PackColor(vertex.color),
        {}};
}

ShaderBone MakeBone(const Matrix4& boneMatrix)
{
    ShaderBone bone = {};
    Matrix4 normalMatrix = MakeNormalMatrix(boneMatrix);
    for (std::size_t row = 0; row < bone.pointMatrix.size(); ++row)
    {
        bone.pointMatrix[row] = boneMatrix.rows[row];
        bone.normalMatrix[row] = normalMatrix.rows[row];
    }
    return bone;
}

}
