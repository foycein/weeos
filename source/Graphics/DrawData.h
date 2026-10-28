#pragma once

#include "Graphics/GraphicsDevice.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Graphics
{

// How many quads and how many stages one picture can hold. The shaders read them from buffers of this size.
constexpr std::size_t MaximumDrawsPerPicture = 8192;
constexpr std::size_t MaximumStagesPerPicture = 32768;

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
};

static_assert(sizeof(ShaderStage) == 22 * sizeof(std::uint32_t));
static_assert(sizeof(ShaderDrawRecord) == 336);
static_assert(offsetof(ShaderDrawRecord, constantColors) == 48);
static_assert(offsetof(ShaderDrawRecord, cornerPositions) == 112);
static_assert(offsetof(ShaderDrawRecord, cornerColors) == 312);

ShaderStage PackStage(const TevStage& stage);

ShaderDrawRecord MakeDrawRecord(const Quad& quad, const Material& material, std::uint32_t firstStage, std::uint32_t stageCount);

}
