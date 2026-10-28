#include "Graphics/MaterialCheck.h"

#include "Graphics/GraphicsError.h"

#include <format>
#include <initializer_list>

namespace Graphics
{

namespace
{

constexpr int LowestRegisterValue = -1024;
constexpr int HighestRegisterValue = 1023;

constexpr std::uint8_t HighestColorInput = static_cast<std::uint8_t>(TevColorInput::Zero);
constexpr std::uint8_t HighestAlphaInput = static_cast<std::uint8_t>(TevAlphaInput::Zero);
constexpr std::uint8_t HighestBias = static_cast<std::uint8_t>(TevBias::SubtractHalf);
constexpr std::uint8_t HighestScale = static_cast<std::uint8_t>(TevScale::Half);
constexpr std::uint8_t HighestRegister = static_cast<std::uint8_t>(TevRegister::Register2);

constexpr std::uint8_t LastFraction = static_cast<std::uint8_t>(TevConstant::OneEighth);
constexpr std::uint8_t FirstWholeColor = static_cast<std::uint8_t>(TevConstant::Color0);
constexpr std::uint8_t FirstSingleChannel = static_cast<std::uint8_t>(TevConstant::Color0Red);
constexpr std::uint8_t LastSingleChannel = static_cast<std::uint8_t>(TevConstant::Color3Alpha);

void Require(bool isTrue, std::size_t stageNumber, const char* problem)
{
    if (!isTrue)
        throw GraphicsError(std::format("Stage {} of the material {}", stageNumber, problem));
}

bool IsFraction(TevConstant constant)
{
    return static_cast<std::uint8_t>(constant) <= LastFraction;
}

bool IsWholeColor(TevConstant constant)
{
    std::uint8_t number = static_cast<std::uint8_t>(constant);
    return number >= FirstWholeColor && number < FirstSingleChannel;
}

bool IsSingleChannel(TevConstant constant)
{
    std::uint8_t number = static_cast<std::uint8_t>(constant);
    return number >= FirstSingleChannel && number <= LastSingleChannel;
}

bool IsColorOperation(TevOperation operation)
{
    switch (operation)
    {
    case TevOperation::Add:
    case TevOperation::Subtract:
    case TevOperation::RedGreater:
    case TevOperation::RedEqual:
    case TevOperation::GreenRedGreater:
    case TevOperation::GreenRedEqual:
    case TevOperation::BlueGreenRedGreater:
    case TevOperation::BlueGreenRedEqual:
    case TevOperation::EachGreater:
    case TevOperation::EachEqual:
        return true;
    }
    return false;
}

bool IsAlphaOperation(TevOperation operation)
{
    switch (operation)
    {
    case TevOperation::Add:
    case TevOperation::Subtract:
    case TevOperation::EachGreater:
    case TevOperation::EachEqual:
        return true;
    default:
        return false;
    }
}

void CheckResultSettings(TevBias bias, TevScale scale, TevRegister output, std::size_t stageNumber)
{
    Require(static_cast<std::uint8_t>(bias) <= HighestBias, stageNumber, "has a bias that does not exist");
    Require(static_cast<std::uint8_t>(scale) <= HighestScale, stageNumber, "has a scale that does not exist");
    Require(static_cast<std::uint8_t>(output) <= HighestRegister, stageNumber, "has an output register that does not exist");
}

void CheckColorCalculation(const TevColorCalculation& color, std::size_t stageNumber)
{
    for (TevColorInput input : {color.inputA, color.inputB, color.inputC, color.inputD})
        Require(static_cast<std::uint8_t>(input) <= HighestColorInput, stageNumber, "has a color input that does not exist");

    Require(IsColorOperation(color.operation), stageNumber, "has a color operation that does not exist");
    CheckResultSettings(color.bias, color.scale, color.output, stageNumber);
}

void CheckAlphaCalculation(const TevAlphaCalculation& alpha, std::size_t stageNumber)
{
    for (TevAlphaInput input : {alpha.inputA, alpha.inputB, alpha.inputC, alpha.inputD})
        Require(static_cast<std::uint8_t>(input) <= HighestAlphaInput, stageNumber, "has an alpha input that does not exist");

    Require(IsAlphaOperation(alpha.operation), stageNumber, "has an alpha operation that an alpha stage cannot do");
    CheckResultSettings(alpha.bias, alpha.scale, alpha.output, stageNumber);
}

void CheckConstants(const TevStage& stage, std::size_t stageNumber)
{
    bool colorIsUsable = IsFraction(stage.constantColor) || IsWholeColor(stage.constantColor) || IsSingleChannel(stage.constantColor);
    Require(colorIsUsable, stageNumber, "has a constant color that does not exist");

    bool alphaIsUsable = IsFraction(stage.constantAlpha) || IsSingleChannel(stage.constantAlpha);
    Require(alphaIsUsable, stageNumber, "has a constant alpha that is neither a fraction nor a single channel");
}

void CheckStage(const TevStage& stage, std::size_t stageNumber, std::size_t textureCount)
{
    bool readsNoTexture = stage.textureIndex == NoTexture;
    Require(readsNoTexture || stage.textureIndex < textureCount, stageNumber, "reads a texture the material does not have");

    CheckColorCalculation(stage.color, stageNumber);
    CheckAlphaCalculation(stage.alpha, stageNumber);
    CheckConstants(stage, stageNumber);
}

void CheckRegisterColor(const RegisterColor& color, std::size_t registerNumber)
{
    for (int value : {color.red, color.green, color.blue, color.alpha})
    {
        if (value < LowestRegisterValue || value > HighestRegisterValue)
        {
            throw GraphicsError(std::format("Color register {} holds {}, which is outside the range {} to {}", registerNumber, value,
                LowestRegisterValue, HighestRegisterValue));
        }
    }
}

}

void CheckMaterial(const Material& material)
{
    if (material.textures.size() > MaximumTextureCount)
    {
        throw GraphicsError(
            std::format("A material has {} textures but at most {} are possible", material.textures.size(), MaximumTextureCount));
    }

    for (const MaterialTexture& materialTexture : material.textures)
    {
        if (!materialTexture.texture.IsValid())
            throw GraphicsError("A material uses a texture that was never made");
    }

    if (material.stages.size() > MaximumStageCount)
    {
        throw GraphicsError(
            std::format("A material has {} stages but at most {} are possible", material.stages.size(), MaximumStageCount));
    }

    for (std::size_t stageNumber = 0; stageNumber < material.stages.size(); ++stageNumber)
        CheckStage(material.stages[stageNumber], stageNumber, material.textures.size());

    for (std::size_t registerNumber = 0; registerNumber < material.colorRegisters.size(); ++registerNumber)
        CheckRegisterColor(material.colorRegisters[registerNumber], registerNumber);

    bool blendModeExists = material.blendMode == BlendMode::StandardAlpha || material.blendMode == BlendMode::Replace;
    if (!blendModeExists)
        throw GraphicsError("A material has a blend mode that does not exist");
}

}
