#include "Graphics/StageList.h"

#include <array>

namespace Graphics
{

namespace
{

constexpr std::array<TevConstant, MaximumTextureCount> WeightOfTexture = {
    TevConstant::Color3Alpha,
    TevConstant::Color3Blue,
    TevConstant::Color3Green,
};

// color = REG1 * raster color, alpha = REG1 alpha * raster alpha
TevStage RasterTintedByRegister1Stage()
{
    TevStage stage;
    stage.color.inputB = TevColorInput::Register1Color;
    stage.color.inputC = TevColorInput::RasterColor;
    stage.alpha.inputB = TevAlphaInput::Register1Alpha;
    stage.alpha.inputC = TevAlphaInput::RasterAlpha;
    return stage;
}

// color = lerp(REG0, REG1, texture color), alpha = lerp(REG0 alpha, REG1 alpha, texture alpha)
TevStage TextureBetweenRegistersStage(std::uint8_t textureIndex)
{
    TevStage stage;
    stage.textureIndex = textureIndex;
    stage.usesRasterColor = false;
    stage.color.inputA = TevColorInput::Register0Color;
    stage.color.inputB = TevColorInput::Register1Color;
    stage.color.inputC = TevColorInput::TextureColor;
    stage.alpha.inputA = TevAlphaInput::Register0Alpha;
    stage.alpha.inputB = TevAlphaInput::Register1Alpha;
    stage.alpha.inputC = TevAlphaInput::TextureAlpha;
    return stage;
}

// color = texture color, alpha = texture alpha
TevStage PlainTextureStage(std::uint8_t textureIndex)
{
    TevStage stage;
    stage.textureIndex = textureIndex;
    stage.usesRasterColor = false;
    stage.color.inputD = TevColorInput::TextureColor;
    stage.alpha.inputD = TevAlphaInput::TextureAlpha;
    return stage;
}

// color = lerp(texture color, previous color, weight), and the same for alpha
TevStage TextureMixedWithPreviousStage(std::uint8_t textureIndex, TevConstant weight)
{
    TevStage stage;
    stage.textureIndex = textureIndex;
    stage.usesRasterColor = false;
    stage.constantColor = weight;
    stage.constantAlpha = weight;
    stage.color.inputA = TevColorInput::TextureColor;
    stage.color.inputB = TevColorInput::PreviousColor;
    stage.color.inputC = TevColorInput::ConstantColor;
    stage.alpha.inputA = TevAlphaInput::TextureAlpha;
    stage.alpha.inputB = TevAlphaInput::PreviousAlpha;
    stage.alpha.inputC = TevAlphaInput::ConstantAlpha;
    return stage;
}

// color = texture color * weight, plus the previous color when there is one, and the same for alpha
TevStage WeightedTextureAddedStage(std::uint8_t textureIndex, TevConstant weight, bool addsToPrevious)
{
    TevStage stage;
    stage.textureIndex = textureIndex;
    stage.usesRasterColor = false;
    stage.constantColor = weight;
    stage.constantAlpha = weight;
    stage.color.inputB = TevColorInput::TextureColor;
    stage.color.inputC = TevColorInput::ConstantColor;
    stage.alpha.inputB = TevAlphaInput::TextureAlpha;
    stage.alpha.inputC = TevAlphaInput::ConstantAlpha;

    if (addsToPrevious)
    {
        stage.color.inputD = TevColorInput::PreviousColor;
        stage.alpha.inputD = TevAlphaInput::PreviousAlpha;
    }
    return stage;
}

// color = lerp(REG0, REG1, previous color), alpha = lerp(REG0 alpha, REG1 alpha, previous alpha)
TevStage PreviousBetweenRegistersStage()
{
    TevStage stage;
    stage.usesRasterColor = false;
    stage.color.inputA = TevColorInput::Register0Color;
    stage.color.inputB = TevColorInput::Register1Color;
    stage.color.inputC = TevColorInput::PreviousColor;
    stage.alpha.inputA = TevAlphaInput::Register0Alpha;
    stage.alpha.inputB = TevAlphaInput::Register1Alpha;
    stage.alpha.inputC = TevAlphaInput::PreviousAlpha;
    return stage;
}

// color = previous color * raster color, alpha = previous alpha * raster alpha
TevStage PreviousTimesRasterStage()
{
    TevStage stage;
    stage.color.inputB = TevColorInput::PreviousColor;
    stage.color.inputC = TevColorInput::RasterColor;
    stage.alpha.inputB = TevAlphaInput::PreviousAlpha;
    stage.alpha.inputC = TevAlphaInput::RasterAlpha;
    return stage;
}

bool RegistersKeepBlackAndWhite(const Material& material)
{
    const RegisterColor& black = material.colorRegisters[0];
    const RegisterColor& white = material.colorRegisters[1];

    bool blackIsBlack = black.red == 0 && black.green == 0 && black.blue == 0 && black.alpha == 0;
    bool whiteIsWhite = white.red == 255 && white.green == 255 && white.blue == 255 && white.alpha == 255;
    return blackIsBlack && whiteIsWhite;
}

std::vector<TevStage> MixTexturesStages(std::size_t textureCount)
{
    if (textureCount == 1)
        return {TextureBetweenRegistersStage(0)};

    if (textureCount == 2)
        return {PlainTextureStage(0), TextureMixedWithPreviousStage(1, WeightOfTexture[0])};

    std::vector<TevStage> stages;
    for (std::size_t textureIndex = 0; textureIndex < textureCount; ++textureIndex)
    {
        std::uint8_t textureNumber = static_cast<std::uint8_t>(textureIndex);
        bool isFirstTexture = textureIndex == 0;
        stages.push_back(WeightedTextureAddedStage(textureNumber, WeightOfTexture[textureIndex], !isFirstTexture));
    }
    return stages;
}

std::vector<TevStage> StandardChain(const Material& material)
{
    std::size_t textureCount = material.textures.size();
    if (textureCount == 0)
        return {RasterTintedByRegister1Stage()};

    std::vector<TevStage> stages = MixTexturesStages(textureCount);

    if (textureCount > 1 && !RegistersKeepBlackAndWhite(material))
        stages.push_back(PreviousBetweenRegistersStage());

    stages.push_back(PreviousTimesRasterStage());
    return stages;
}

}

std::vector<TevStage> ChooseStages(const Material& material)
{
    if (!material.stages.empty())
        return material.stages;

    return StandardChain(material);
}

}
