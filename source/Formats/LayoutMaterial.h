#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/SectionedFile.h"
#include "Formats/TplTexture.h"
#include "Formats/Vectors.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Formats
{

// A color whose parts are signed 16 bit numbers, the way the registers of the graphics chip hold them.
struct SignedColor
{
    std::int16_t red;
    std::int16_t green;
    std::int16_t blue;
    std::int16_t alpha;
};

struct MaterialTexture
{
    // The position in the texture list of the layout.
    std::uint16_t textureIndex;

    TextureWrap horizontalWrap;
    TextureWrap verticalWrap;

    // Both filters are 0 in every layout of the System Menu.
    std::uint8_t minificationFilter;
    std::uint8_t magnificationFilter;
};

struct TextureTransform
{
    Vector2 translation;
    float rotation;
    Vector2 scale;
};

struct TextureCoordinateGenerator
{
    std::uint8_t matrixType;
    std::uint8_t source;
    std::uint8_t matrixSource;
};

enum class ColorSource : std::uint8_t
{
    Material = 0,
    Vertex = 1,
};

struct ChannelControl
{
    ColorSource colorSource;
    ColorSource alphaSource;
};

constexpr std::size_t TevStageSize = 16;
constexpr std::size_t TevSwapTableSize = 4;

// One stage of the graphics chip that combines colors. The format documents no fields, so the 16 bytes are kept as they are.
using TevStage = std::array<Byte, TevStageSize>;

// An indirect texture matrix (offset, rotation and scale, five floats) and an indirect stage (which coordinates and texture move
// the picture, and how much).
constexpr std::size_t IndirectMatrixSize = 20;
constexpr std::size_t IndirectStageSize = 4;
using IndirectMatrix = std::array<Byte, IndirectMatrixSize>;
using IndirectStage = std::array<Byte, IndirectStageSize>;

using TevSwapTable = std::array<Byte, TevSwapTableSize>;

struct AlphaCompare
{
    std::uint8_t firstCondition;
    std::uint8_t secondCondition;
    std::uint8_t operation;
    std::uint8_t firstReference;
    std::uint8_t secondReference;
};

struct BlendMode
{
    std::uint8_t function;
    std::uint8_t sourceFactor;
    std::uint8_t destinationFactor;
    std::uint8_t logicOperation;
};

constexpr std::size_t MaterialConstantColorCount = 4;

// A material tells how a picture or a window frame is drawn. The parts after the colors are
// optional: the material says in a field of flags which of them it has.
struct Material
{
    std::string name;

    SignedColor foreColor;
    SignedColor backColor;
    SignedColor colorRegister3;
    std::array<Rgba8Color, MaterialConstantColorCount> constantColors;

    std::vector<MaterialTexture> textures;
    std::vector<TextureTransform> textureTransforms;
    std::vector<TextureCoordinateGenerator> textureCoordinateGenerators;
    std::optional<ChannelControl> channelControl;
    std::optional<Rgba8Color> materialColor;
    std::optional<TevSwapTable> tevSwapTable;

    // Only the water of the Wii Speak Channel's chat room has them; WeeOS draws that material without moving its picture
    // (docs/audit/WiiSpeak.md).
    std::vector<IndirectMatrix> indirectMatrices;
    std::vector<IndirectStage> indirectStages;
    std::vector<TevStage> tevStages;
    std::optional<AlphaCompare> alphaCompare;
    std::optional<BlendMode> blendMode;
};

// Reads the materials of the "mat1" section of a layout.
std::vector<Material> ReadMaterials(const Section& section);

}
