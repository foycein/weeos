#include "Layout/DefaultTevStages.h"

#include "Layout/LayoutError.h"

#include "Graphics/GraphicsDevice.h"

#include <array>
#include <cstdint>
#include <format>

namespace Layout
{

namespace
{

using Graphics::TevAlphaInput;
using Graphics::TevColorInput;
using Graphics::TevConstant;

constexpr std::uint8_t FirstTexture = 0;
constexpr std::uint8_t SecondTexture = 1;
constexpr std::size_t TwoTextures = 2;

// Each texture of a material with three or more textures is weighted by one channel of the constant colors 3 and 2.
constexpr std::array<TevConstant, 8> TextureWeights = {
    TevConstant::Color3Alpha, TevConstant::Color3Blue, TevConstant::Color3Green, TevConstant::Color3Red,
    TevConstant::Color2Alpha, TevConstant::Color2Blue, TevConstant::Color2Green, TevConstant::Color2Red,
};

// Each result is (D + (1 - C) * A + C * B): D is added, and C chooses between A and B.
void SetColorInputs(Graphics::TevStage& stage, TevColorInput a, TevColorInput b, TevColorInput c, TevColorInput d)
{
    stage.color.inputA = a;
    stage.color.inputB = b;
    stage.color.inputC = c;
    stage.color.inputD = d;
}

void SetAlphaInputs(Graphics::TevStage& stage, TevAlphaInput a, TevAlphaInput b, TevAlphaInput c, TevAlphaInput d)
{
    stage.alpha.inputA = a;
    stage.alpha.inputB = b;
    stage.alpha.inputC = c;
    stage.alpha.inputD = d;
}

Graphics::TevStage MakeStage(std::uint8_t textureIndex, bool usesRasterColor)
{
    Graphics::TevStage stage;
    stage.textureIndex = textureIndex;
    stage.usesRasterColor = usesRasterColor;
    return stage;
}

// register 1 times the raster color
Graphics::TevStage MakeRegister1TimesRasterStage()
{
    Graphics::TevStage stage = MakeStage(Graphics::NoTexture, true);
    SetColorInputs(stage, TevColorInput::Zero, TevColorInput::Register1Color, TevColorInput::RasterColor, TevColorInput::Zero);
    SetAlphaInputs(stage, TevAlphaInput::Zero, TevAlphaInput::Register1Alpha, TevAlphaInput::RasterAlpha, TevAlphaInput::Zero);
    return stage;
}

// the texture, going from register 0 where the texture is 0 to register 1 where the texture is 1
Graphics::TevStage MakeTintedTextureStage()
{
    Graphics::TevStage stage = MakeStage(FirstTexture, false);
    SetColorInputs(stage, TevColorInput::Register0Color, TevColorInput::Register1Color, TevColorInput::TextureColor, TevColorInput::Zero);
    SetAlphaInputs(stage, TevAlphaInput::Register0Alpha, TevAlphaInput::Register1Alpha, TevAlphaInput::TextureAlpha, TevAlphaInput::Zero);
    return stage;
}

// only the texture
Graphics::TevStage MakeTextureOnlyStage(std::uint8_t textureIndex)
{
    Graphics::TevStage stage = MakeStage(textureIndex, false);
    SetColorInputs(stage, TevColorInput::Zero, TevColorInput::Zero, TevColorInput::Zero, TevColorInput::TextureColor);
    SetAlphaInputs(stage, TevAlphaInput::Zero, TevAlphaInput::Zero, TevAlphaInput::Zero, TevAlphaInput::TextureAlpha);
    return stage;
}

// the texture where the constant is 0, and the previous result where the constant is 1
Graphics::TevStage MakeBlendWithPreviousStage(std::uint8_t textureIndex, TevConstant constant)
{
    Graphics::TevStage stage = MakeStage(textureIndex, false);
    SetColorInputs(stage, TevColorInput::TextureColor, TevColorInput::PreviousColor, TevColorInput::ConstantColor, TevColorInput::Zero);
    SetAlphaInputs(stage, TevAlphaInput::TextureAlpha, TevAlphaInput::PreviousAlpha, TevAlphaInput::ConstantAlpha, TevAlphaInput::Zero);
    stage.constantColor = constant;
    stage.constantAlpha = constant;
    return stage;
}

// the previous result, plus the texture times the constant
Graphics::TevStage MakeWeightedTextureStage(std::uint8_t textureIndex, TevConstant weight)
{
    TevColorInput previousColor = textureIndex == FirstTexture ? TevColorInput::Zero : TevColorInput::PreviousColor;
    TevAlphaInput previousAlpha = textureIndex == FirstTexture ? TevAlphaInput::Zero : TevAlphaInput::PreviousAlpha;

    Graphics::TevStage stage = MakeStage(textureIndex, false);
    SetColorInputs(stage, TevColorInput::Zero, TevColorInput::TextureColor, TevColorInput::ConstantColor, previousColor);
    SetAlphaInputs(stage, TevAlphaInput::Zero, TevAlphaInput::TextureAlpha, TevAlphaInput::ConstantAlpha, previousAlpha);
    stage.constantColor = weight;
    stage.constantAlpha = weight;
    return stage;
}

// the previous result, going from register 0 where it is 0 to register 1 where it is 1
Graphics::TevStage MakeTintPreviousStage()
{
    Graphics::TevStage stage = MakeStage(Graphics::NoTexture, false);
    SetColorInputs(stage, TevColorInput::Register0Color, TevColorInput::Register1Color, TevColorInput::PreviousColor, TevColorInput::Zero);
    SetAlphaInputs(stage, TevAlphaInput::Register0Alpha, TevAlphaInput::Register1Alpha, TevAlphaInput::PreviousAlpha, TevAlphaInput::Zero);
    return stage;
}

// the previous result times the raster color
Graphics::TevStage MakePreviousTimesRasterStage()
{
    Graphics::TevStage stage = MakeStage(Graphics::NoTexture, true);
    SetColorInputs(stage, TevColorInput::Zero, TevColorInput::PreviousColor, TevColorInput::RasterColor, TevColorInput::Zero);
    SetAlphaInputs(stage, TevAlphaInput::Zero, TevAlphaInput::PreviousAlpha, TevAlphaInput::RasterAlpha, TevAlphaInput::Zero);
    return stage;
}

void AddStagesForSeveralTextures(std::vector<Graphics::TevStage>& stages, std::size_t textureCount)
{
    if (textureCount == TwoTextures)
    {
        stages.push_back(MakeTextureOnlyStage(FirstTexture));
        stages.push_back(MakeBlendWithPreviousStage(SecondTexture, TextureWeights[FirstTexture]));
        return;
    }

    for (std::size_t textureIndex = 0; textureIndex < textureCount; ++textureIndex)
        stages.push_back(MakeWeightedTextureStage(static_cast<std::uint8_t>(textureIndex), TextureWeights[textureIndex]));
}

}

std::vector<Graphics::TevStage> MakeDefaultTevStages(std::size_t textureCount, bool tintsTextures, bool multipliesByRasterColor)
{
    if (textureCount > Graphics::MaximumTextureCount)
        throw LayoutError(std::format("A material has {} textures, but at most {} are supported", textureCount, Graphics::MaximumTextureCount));

    if (textureCount == 0)
        return {MakeRegister1TimesRasterStage()};

    std::vector<Graphics::TevStage> stages;
    if (textureCount == 1)
    {
        stages.push_back(MakeTintedTextureStage());
    }
    else
    {
        AddStagesForSeveralTextures(stages, textureCount);
        if (tintsTextures)
            stages.push_back(MakeTintPreviousStage());
    }

    if (multipliesByRasterColor)
        stages.push_back(MakePreviousTimesRasterStage());

    return stages;
}

}
