#include "Layout/TevStageConversion.h"

#include "Formats/BitFields.h"
#include "Layout/LayoutError.h"

#include "Graphics/GraphicsDevice.h"

#include <format>

namespace Layout
{

namespace
{

// The bytes of a stage record.
constexpr std::size_t TextureCoordinateByte = 0;
constexpr std::size_t ColorChannelByte = 1;
constexpr std::size_t TextureMapByte = 2;
constexpr std::size_t TextureMapAndSwapByte = 3;
constexpr std::size_t ColorInputsABByte = 4;
constexpr std::size_t ColorInputsCDByte = 5;
constexpr std::size_t ColorOperationByte = 6;
constexpr std::size_t ColorOutputByte = 7;
constexpr std::size_t AlphaInputsABByte = 8;
constexpr std::size_t AlphaInputsCDByte = 9;
constexpr std::size_t AlphaOperationByte = 10;
constexpr std::size_t AlphaOutputByte = 11;
constexpr std::size_t IndirectStageByte = 12;
constexpr std::size_t IndirectBiasAndMatrixByte = 13;
constexpr std::size_t IndirectWrapByte = 14;
constexpr std::size_t IndirectFormatByte = 15;

// The indirect part: the bias of S, T and U in three bits and the matrix in the next four (0 none, 1 to 3 the matrices 0 to 2).
constexpr int IndirectBiasBit = 0;
constexpr int IndirectBiasBitCount = 3;
constexpr int IndirectMatrixBit = 3;
constexpr int IndirectMatrixBitCount = 4;
constexpr unsigned NoIndirectMatrix = 0;
constexpr unsigned FirstIndirectMatrix = 1;
constexpr unsigned BiasOnSTAndU = 7;

constexpr Formats::Byte NoneNumber = 0xFF;
constexpr Formats::Byte RasterColorChannel = 4;

constexpr int SingleBitCount = 1;

// Two inputs share a byte: the first one in the low half and the second one in the high half.
constexpr int LowHalfBit = 0;
constexpr int HighHalfBit = 4;
constexpr int HalfByteBitCount = 4;

// The bits of the byte with the texture map and the swap tables.
constexpr int TextureMapHighBit = 0;
constexpr int SwapTablesBit = 1;
constexpr int SwapTablesBitCount = 4;

// The bits of the operation byte.
constexpr int OperationBit = 0;
constexpr int OperationBitCount = 4;
constexpr int BiasBit = 4;
constexpr int BiasBitCount = 2;
constexpr int ScaleBit = 6;
constexpr int ScaleBitCount = 2;

// The bits of the output byte.
constexpr int ClampBit = 0;
constexpr int OutputRegisterBit = 1;
constexpr int OutputRegisterBitCount = 2;
constexpr int ConstantSelectionBit = 3;
constexpr int ConstantSelectionBitCount = 5;

constexpr unsigned HighestAlphaInput = 7;
constexpr unsigned HighestBias = 2;
constexpr unsigned HighestFraction = 7;
constexpr unsigned FirstConstantColor = 12;
constexpr unsigned FirstConstantChannel = 16;

[[noreturn]] void FailStage(const std::string& problem)
{
    throw LayoutError(problem);
}

unsigned LowHalf(Formats::Byte byte)
{
    return Formats::ExtractBits(byte, LowHalfBit, HalfByteBitCount);
}

unsigned HighHalf(Formats::Byte byte)
{
    return Formats::ExtractBits(byte, HighHalfBit, HalfByteBitCount);
}

// A swap table byte holds two bits for each channel, red in the lowest bits.
std::array<std::uint8_t, 4> ReadSwapTable(Formats::Byte table)
{
    constexpr int BitsPerChannel = 2;
    std::array<std::uint8_t, 4> swap = {};
    for (std::size_t channel = 0; channel < swap.size(); ++channel)
        swap[channel] = static_cast<std::uint8_t>(Formats::ExtractBits(table, static_cast<int>(channel) * BitsPerChannel, BitsPerChannel));
    return swap;
}

// The stage names its raster table in two bits and its texture table in the next two.
void ReadSwaps(const Formats::TevStage& record, const std::optional<Formats::TevSwapTable>& swapTables, Graphics::TevStage& stage)
{
    constexpr int TableNumberBits = 2;
    unsigned rasterTable = Formats::ExtractBits(record[TextureMapAndSwapByte], SwapTablesBit, TableNumberBits);
    unsigned textureTable = Formats::ExtractBits(record[TextureMapAndSwapByte], SwapTablesBit + TableNumberBits, TableNumberBits);
    if (!swapTables)
    {
        if (rasterTable != 0 || textureTable != 0)
            FailStage("a stage uses a swap table that its material does not have");
        return;
    }

    stage.rasterSwap = ReadSwapTable((*swapTables)[rasterTable]);
    stage.textureSwap = ReadSwapTable((*swapTables)[textureTable]);
}

// The stage reads the texture with the same number as its texture map, and the quad supplies the coordinates with that number too.
std::uint8_t ReadTextureIndex(const Formats::TevStage& record)
{
    bool hasHighBit = Formats::ExtractBits(record[TextureMapAndSwapByte], TextureMapHighBit, SingleBitCount) != 0;
    if (record[TextureMapByte] == NoneNumber && !hasHighBit)
        return Graphics::NoTexture;

    std::size_t textureMap = record[TextureMapByte];
    if (hasHighBit || textureMap >= Graphics::MaximumTextureCount)
        FailStage(std::format("a stage reads the texture map {}", textureMap));

    if (record[TextureCoordinateByte] != textureMap)
    {
        FailStage(std::format("a stage reads the texture map {} with the texture coordinates {}",
            textureMap, record[TextureCoordinateByte]));
    }

    return record[TextureMapByte];
}

bool ReadUsesRasterColor(const Formats::TevStage& record)
{
    if (record[ColorChannelByte] == NoneNumber)
        return false;

    if (record[ColorChannelByte] == RasterColorChannel)
        return true;

    FailStage(std::format("a stage reads the color channel {}", record[ColorChannelByte]));
}

Graphics::TevColorInput ReadColorInput(unsigned number)
{
    return static_cast<Graphics::TevColorInput>(number);
}

Graphics::TevAlphaInput ReadAlphaInput(unsigned number)
{
    if (number > HighestAlphaInput)
        FailStage(std::format("a stage uses the alpha input {}", number));

    return static_cast<Graphics::TevAlphaInput>(number);
}

Graphics::TevOperation ReadOperation(unsigned number, bool isAlpha)
{
    bool isCompare = number >= static_cast<unsigned>(Graphics::TevOperation::RedGreater);
    bool isAddOrSubtract = number <= static_cast<unsigned>(Graphics::TevOperation::Subtract);
    bool isAlphaCompare = number == static_cast<unsigned>(Graphics::TevOperation::EachGreater)
        || number == static_cast<unsigned>(Graphics::TevOperation::EachEqual);

    bool isKnown = isAddOrSubtract || (isCompare && (!isAlpha || isAlphaCompare));
    if (!isKnown)
        FailStage(std::format("a stage uses the operation {}", number));

    return static_cast<Graphics::TevOperation>(number);
}

Graphics::TevBias ReadBias(unsigned number)
{
    if (number > HighestBias)
        FailStage(std::format("a stage uses the bias {}", number));

    return static_cast<Graphics::TevBias>(number);
}

Graphics::TevColorCalculation ReadColorCalculation(const Formats::TevStage& record)
{
    Formats::Byte operationByte = record[ColorOperationByte];
    Formats::Byte outputByte = record[ColorOutputByte];

    Graphics::TevColorCalculation color;
    color.inputA = ReadColorInput(LowHalf(record[ColorInputsABByte]));
    color.inputB = ReadColorInput(HighHalf(record[ColorInputsABByte]));
    color.inputC = ReadColorInput(LowHalf(record[ColorInputsCDByte]));
    color.inputD = ReadColorInput(HighHalf(record[ColorInputsCDByte]));
    color.operation = ReadOperation(Formats::ExtractBits(operationByte, OperationBit, OperationBitCount), false);
    color.bias = ReadBias(Formats::ExtractBits(operationByte, BiasBit, BiasBitCount));
    color.scale = static_cast<Graphics::TevScale>(Formats::ExtractBits(operationByte, ScaleBit, ScaleBitCount));
    color.clamp = Formats::ExtractBits(outputByte, ClampBit, SingleBitCount) != 0;
    color.output = static_cast<Graphics::TevRegister>(Formats::ExtractBits(outputByte, OutputRegisterBit, OutputRegisterBitCount));
    return color;
}

Graphics::TevAlphaCalculation ReadAlphaCalculation(const Formats::TevStage& record)
{
    Formats::Byte operationByte = record[AlphaOperationByte];
    Formats::Byte outputByte = record[AlphaOutputByte];

    Graphics::TevAlphaCalculation alpha;
    alpha.inputA = ReadAlphaInput(LowHalf(record[AlphaInputsABByte]));
    alpha.inputB = ReadAlphaInput(HighHalf(record[AlphaInputsABByte]));
    alpha.inputC = ReadAlphaInput(LowHalf(record[AlphaInputsCDByte]));
    alpha.inputD = ReadAlphaInput(HighHalf(record[AlphaInputsCDByte]));
    alpha.operation = ReadOperation(Formats::ExtractBits(operationByte, OperationBit, OperationBitCount), true);
    alpha.bias = ReadBias(Formats::ExtractBits(operationByte, BiasBit, BiasBitCount));
    alpha.scale = static_cast<Graphics::TevScale>(Formats::ExtractBits(operationByte, ScaleBit, ScaleBitCount));
    alpha.clamp = Formats::ExtractBits(outputByte, ClampBit, SingleBitCount) != 0;
    alpha.output = static_cast<Graphics::TevRegister>(Formats::ExtractBits(outputByte, OutputRegisterBit, OutputRegisterBitCount));
    return alpha;
}

// The numbers 0 to 7 are fractions, 12 to 15 are the constant colors and 16 to 31 are one channel of a constant color.
Graphics::TevConstant ReadConstantColor(const Formats::TevStage& record)
{
    unsigned number = Formats::ExtractBits(record[ColorOutputByte], ConstantSelectionBit, ConstantSelectionBitCount);
    bool isFraction = number <= HighestFraction;
    bool isColorOrChannel = number >= FirstConstantColor;
    if (!isFraction && !isColorOrChannel)
        FailStage(std::format("a stage uses the constant color {}", number));

    return static_cast<Graphics::TevConstant>(number);
}

// The constant of the alpha is a fraction or one channel of a constant color, but not a whole color.
Graphics::TevConstant ReadConstantAlpha(const Formats::TevStage& record)
{
    unsigned number = Formats::ExtractBits(record[AlphaOutputByte], ConstantSelectionBit, ConstantSelectionBitCount);
    bool isFraction = number <= HighestFraction;
    bool isChannel = number >= FirstConstantChannel;
    if (!isFraction && !isChannel)
        FailStage(std::format("a stage uses the constant alpha {}", number));

    return static_cast<Graphics::TevConstant>(number);
}

// The last four bytes are the indirect part, which moves the place where the stage reads its texture. WeeOS draws what the water of
// the Wii Speak Channel's chat room uses: the indirect stage 0 of 8 bit texels less 128, through the first indirect matrix, without
// wrapping, adding or alpha (docs/audit/WiiSpeak.md).
bool ReadIsMovedByIndirectTexture(const Formats::TevStage& record)
{
    unsigned matrix = Formats::ExtractBits(record[IndirectBiasAndMatrixByte], IndirectMatrixBit, IndirectMatrixBitCount);
    bool hasOtherIndirectSettings = record[IndirectStageByte] != 0 || record[IndirectWrapByte] != 0 || record[IndirectFormatByte] != 0;
    if (matrix == NoIndirectMatrix && !hasOtherIndirectSettings)
        return false;

    unsigned bias = Formats::ExtractBits(record[IndirectBiasAndMatrixByte], IndirectBiasBit, IndirectBiasBitCount);
    if (matrix != FirstIndirectMatrix || bias != BiasOnSTAndU || hasOtherIndirectSettings)
        FailStage("a stage uses an indirect texture in a way that WeeOS does not draw");

    return true;
}

}

Graphics::TevStage ConvertTevStage(const Formats::TevStage& record, const std::optional<Formats::TevSwapTable>& swapTables)
{
    Graphics::TevStage stage;
    ReadSwaps(record, swapTables, stage);
    stage.textureIndex = ReadTextureIndex(record);
    stage.usesRasterColor = ReadUsesRasterColor(record);
    stage.constantColor = ReadConstantColor(record);
    stage.constantAlpha = ReadConstantAlpha(record);
    stage.color = ReadColorCalculation(record);
    stage.alpha = ReadAlphaCalculation(record);
    stage.isMovedByIndirectTexture = ReadIsMovedByIndirectTexture(record);
    return stage;
}

}
