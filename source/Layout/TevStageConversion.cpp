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
constexpr std::size_t FirstIndirectByte = 12;

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

void RequireNoIndirectTexture(const Formats::TevStage& record)
{
    for (std::size_t index = FirstIndirectByte; index < record.size(); ++index)
    {
        if (record[index] != 0)
            FailStage("a stage uses an indirect texture");
    }
}

void RequireNoSwapTable(const Formats::TevStage& record)
{
    if (Formats::ExtractBits(record[TextureMapAndSwapByte], SwapTablesBit, SwapTablesBitCount) != 0)
        FailStage("a stage uses a swap table other than the plain one");
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

}

Graphics::TevStage ConvertTevStage(const Formats::TevStage& record)
{
    RequireNoIndirectTexture(record);
    RequireNoSwapTable(record);

    Graphics::TevStage stage;
    stage.textureIndex = ReadTextureIndex(record);
    stage.usesRasterColor = ReadUsesRasterColor(record);
    stage.constantColor = ReadConstantColor(record);
    stage.constantAlpha = ReadConstantAlpha(record);
    stage.color = ReadColorCalculation(record);
    stage.alpha = ReadAlphaCalculation(record);
    return stage;
}

}
