#include "Layout/LiveMaterial.h"

#include "Layout/LayoutError.h"
#include "Layout/TevStageConversion.h"
#include "Layout/TransformMatrix.h"

#include <cmath>
#include <format>

namespace Layout
{

namespace
{

// The numbers of the graphics chip that the generators of the layout files use.
constexpr std::uint8_t GeneratorFromTwoByFourMatrix = 1;
constexpr std::size_t FirstTextureMatrix = 30;
constexpr std::size_t TextureMatrixSpacing = 3;
constexpr std::size_t IdentityMatrix = 60;

constexpr std::uint8_t CompareAlways = 7;

constexpr std::uint8_t BlendNone = 0;
constexpr std::uint8_t BlendMixing = 1;
constexpr std::uint8_t SourceAlphaFactor = 4;
constexpr std::uint8_t InverseSourceAlphaFactor = 5;

constexpr Formats::TevSwapTable PlainSwapTable = {0xE4, 0xE4, 0xE4, 0xE4};

[[noreturn]] void FailMaterial(const Formats::Material& material, const std::string& problem)
{
    throw LayoutError(std::format("The material {} is not supported: {}", material.name, problem));
}

void RequireSupportedTextures(const Formats::Material& material, std::size_t layoutTextureCount)
{
    if (material.textures.size() > Graphics::MaximumTextureCount)
        FailMaterial(material, std::format("it has {} textures, but at most {} are supported", material.textures.size(), Graphics::MaximumTextureCount));

    for (const Formats::MaterialTexture& texture : material.textures)
    {
        if (texture.textureIndex >= layoutTextureCount)
            FailMaterial(material, std::format("it uses the texture {} and the layout lists only {}", texture.textureIndex, layoutTextureCount));
    }
}

bool IsTextureMatrix(std::size_t matrixSource)
{
    return matrixSource >= FirstTextureMatrix && (matrixSource - FirstTextureMatrix) % TextureMatrixSpacing == 0;
}

std::size_t TextureTransformIndexOf(std::size_t matrixSource)
{
    return (matrixSource - FirstTextureMatrix) / TextureMatrixSpacing;
}

void RequireSupportedGenerators(const Formats::Material& material)
{
    if (material.textureCoordinateGenerators.size() != material.textures.size())
    {
        FailMaterial(material, std::format("it has {} textures and {} texture coordinate generators",
            material.textures.size(), material.textureCoordinateGenerators.size()));
    }

    for (const Formats::TextureCoordinateGenerator& generator : material.textureCoordinateGenerators)
    {
        if (generator.matrixType != GeneratorFromTwoByFourMatrix)
            FailMaterial(material, std::format("it has a texture coordinate generator of the type {}", generator.matrixType));

        if (generator.source < TextureCoordinateSourceBase)
            FailMaterial(material, std::format("it has a texture coordinate generator with the source {}", generator.source));

        std::size_t setNumber = generator.source - TextureCoordinateSourceBase;
        if (setNumber >= Graphics::MaximumTextureCount)
            FailMaterial(material, std::format("it reads the set {} of texture coordinates", setNumber));

        if (generator.matrixSource == IdentityMatrix)
            continue;

        if (!IsTextureMatrix(generator.matrixSource))
            FailMaterial(material, std::format("it has a texture coordinate generator with the matrix {}", generator.matrixSource));

        if (TextureTransformIndexOf(generator.matrixSource) >= material.textureTransforms.size())
            FailMaterial(material, std::format("its generator uses the matrix {} but it has only {} texture transforms",
                generator.matrixSource, material.textureTransforms.size()));
    }
}

void RequireSupportedParts(const Formats::Material& material)
{
    if (material.tevSwapTable && *material.tevSwapTable != PlainSwapTable)
        FailMaterial(material, "it has a swap table other than the plain one");

    bool alphaTestAlwaysPasses = !material.alphaCompare
        || (material.alphaCompare->firstCondition == CompareAlways && material.alphaCompare->secondCondition == CompareAlways);
    if (!alphaTestAlwaysPasses)
        FailMaterial(material, "it has an alpha test that can reject pixels");

    if (material.tevStages.size() > Graphics::MaximumStageCount)
        FailMaterial(material, std::format("it has {} stages, but at most {} are supported", material.tevStages.size(), Graphics::MaximumStageCount));
}

Graphics::BlendMode ConvertBlendMode(const Formats::Material& material)
{
    if (!material.blendMode)
        return Graphics::BlendMode::StandardAlpha;

    const Formats::BlendMode& mode = *material.blendMode;
    if (mode.function == BlendNone)
        return Graphics::BlendMode::Replace;

    bool isStandardAlpha = mode.function == BlendMixing && mode.sourceFactor == SourceAlphaFactor
        && mode.destinationFactor == InverseSourceAlphaFactor;
    if (!isStandardAlpha)
        FailMaterial(material, "it blends in another way than with the alpha of the source");

    return Graphics::BlendMode::StandardAlpha;
}

Graphics::TevStage ConvertStage(const Formats::Material& material, const Formats::TevStage& record)
{
    try
    {
        return ConvertTevStage(record);
    }
    catch (const LayoutError& error)
    {
        FailMaterial(material, error.what());
    }
}

std::vector<Graphics::TevStage> ConvertStages(const Formats::Material& material)
{
    std::vector<Graphics::TevStage> stages;
    for (const Formats::TevStage& record : material.tevStages)
    {
        Graphics::TevStage stage = ConvertStage(material, record);

        bool readsATexture = stage.textureIndex != Graphics::NoTexture;
        if (readsATexture && stage.textureIndex >= material.textures.size())
            FailMaterial(material, std::format("a stage reads the texture {} and the material has {}", stage.textureIndex, material.textures.size()));

        stages.push_back(stage);
    }
    return stages;
}

// The texture matrix of the graphics chip moves the center of the texture, so the turn and the scale
// happen around the middle (0.5, 0.5) of the texture, and then the texture is shifted.
Graphics::TextureMatrix MakeTextureMatrix(const Formats::TextureTransform& transform)
{
    float radians = RadiansPerDegree * transform.rotation;
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    float horizontalFromHorizontal = cosine * transform.scale.x;
    float horizontalFromVertical = -sine * transform.scale.y;
    float verticalFromHorizontal = sine * transform.scale.x;
    float verticalFromVertical = cosine * transform.scale.y;

    Graphics::TextureMatrix matrix;
    matrix.rows[0] = {horizontalFromHorizontal, horizontalFromVertical,
        transform.translation.x + 0.5f - 0.5f * (horizontalFromHorizontal + horizontalFromVertical)};
    matrix.rows[1] = {verticalFromHorizontal, verticalFromVertical,
        transform.translation.y + 0.5f - 0.5f * (verticalFromHorizontal + verticalFromVertical)};
    return matrix;
}

}

LiveMaterial MakeLiveMaterial(Formats::Material description, const std::vector<std::string>& layoutTextureNames)
{
    RequireSupportedTextures(description, layoutTextureNames.size());
    RequireSupportedGenerators(description);
    RequireSupportedParts(description);

    LiveMaterial material;
    material.blendMode = ConvertBlendMode(description);
    material.stages = ConvertStages(description);
    for (const Formats::MaterialTexture& texture : description.textures)
        material.textureNames.push_back(layoutTextureNames[texture.textureIndex]);

    material.description = std::move(description);
    return material;
}

std::size_t TextureCoordinateSetOf(const Formats::Material& material, std::size_t textureMapIndex)
{
    return material.textureCoordinateGenerators[textureMapIndex].source - TextureCoordinateSourceBase;
}

Graphics::TextureMatrix TextureMatrixOf(const Formats::Material& material, std::size_t textureMapIndex)
{
    std::size_t matrixSource = material.textureCoordinateGenerators[textureMapIndex].matrixSource;
    if (matrixSource == IdentityMatrix)
        return Graphics::TextureMatrix();

    return MakeTextureMatrix(material.textureTransforms[TextureTransformIndexOf(matrixSource)]);
}

}
