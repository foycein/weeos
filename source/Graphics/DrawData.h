#pragma once

#include "Graphics/GraphicsDevice.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Graphics
{

// How many draws (quads and meshes), stages, meshes and mesh corners one picture can hold. The shaders read them from buffers
// of this size.
constexpr std::size_t MaximumDrawsPerPicture = 8192;
constexpr std::size_t MaximumStagesPerPicture = 32768;
constexpr std::size_t MaximumMeshesPerPicture = 1024;
constexpr std::size_t MaximumMeshCornersPerPicture = 131072;

// The data of a draw as the shaders read it. The structures in this file have exactly the layout of the structures in
// Shaders/DrawData.glsl: when one changes, the other has to change the same way.

struct ShaderColor
{
    float red;
    float green;
    float blue;
    float alpha;
};

// One stage. Every field of TevStage has a number of its own.
struct ShaderStage
{
    std::uint32_t textureIndex;
    std::uint32_t usesRasterColor;
    std::uint32_t constantColor;
    std::uint32_t constantAlpha;

    std::uint32_t colorInputA;
    std::uint32_t colorInputB;
    std::uint32_t colorInputC;
    std::uint32_t colorInputD;
    std::uint32_t colorOperation;
    std::uint32_t colorBias;
    std::uint32_t colorScale;
    std::uint32_t colorClamp;
    std::uint32_t colorOutput;

    std::uint32_t alphaInputA;
    std::uint32_t alphaInputB;
    std::uint32_t alphaInputC;
    std::uint32_t alphaInputD;
    std::uint32_t alphaOperation;
    std::uint32_t alphaBias;
    std::uint32_t alphaScale;
    std::uint32_t alphaClamp;
    std::uint32_t alphaOutput;
};

// One quad with its material. The stages of the draw are ShaderStage structures in the stage buffer, starting at firstStage.
struct ShaderDrawRecord
{
    // The registers and constants are colors in the range 0 to 1, where 1 is the 255 of the material.
    std::array<ShaderColor, ColorRegisterCount> colorRegisters;
    std::array<ShaderColor, ConstantColorCount> constantColors;

    std::array<Formats::Vector2, QuadCornerCount> cornerPositions;
    std::array<std::array<Formats::Vector2, QuadCornerCount>, MaximumTextureCount> textureCoordinates;

    // Per texture the six numbers of the two rows of its TextureMatrix.
    std::array<std::array<float, 6>, MaximumTextureCount> textureMatrices;

    // Red in the lowest byte, then green, blue and alpha in the highest byte.
    std::array<std::uint32_t, QuadCornerCount> cornerColors;

    std::uint32_t firstStage;
    std::uint32_t stageCount;

    std::uint32_t skipsTransparentPixels;

    // For a mesh: its number in the mesh buffer.
    std::uint32_t meshNumber;
    std::array<std::uint32_t, 2> unused;
};

// One mesh: its matrices and its light. The rows of the matrices are in the order of Matrix4.
struct ShaderMeshRecord
{
    std::array<std::array<float, 4>, 3> modelView;
    std::array<std::array<float, 4>, 3> normalMatrix;
    std::array<std::array<float, 4>, 4> projection;
    std::array<float, 4> lightPosition;
    ShaderColor lightColor;
    ShaderColor ambientColor;
    std::uint32_t hasLight;
    std::uint32_t textureCoordinatesFromNormal;
    std::array<std::uint32_t, 2> unused;
};

// One corner of a mesh, in two groups of four numbers.
struct ShaderMeshCorner
{
    Formats::Vector3 position;
    float s;
    Formats::Vector3 normal;
    float t;
};

static_assert(sizeof(ShaderStage) == 22 * sizeof(std::uint32_t));
static_assert(sizeof(ShaderDrawRecord) == 352);
static_assert(sizeof(ShaderMeshRecord) == 224);
static_assert(sizeof(ShaderMeshCorner) == 32);
static_assert(offsetof(ShaderDrawRecord, constantColors) == 48);
static_assert(offsetof(ShaderDrawRecord, cornerPositions) == 112);
static_assert(offsetof(ShaderDrawRecord, cornerColors) == 312);

ShaderStage PackStage(const TevStage& stage);

ShaderDrawRecord MakeDrawRecord(const Quad& quad, const Material& material, std::uint32_t firstStage, std::uint32_t stageCount);

ShaderMeshRecord MakeMeshRecord(const MeshPlacement& placement);

ShaderMeshCorner MakeMeshCorner(const MeshVertex& vertex);

}
