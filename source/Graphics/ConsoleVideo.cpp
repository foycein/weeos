#include "Graphics/ConsoleVideo.h"

#include <array>
#include <cmath>

namespace Graphics
{

namespace
{

constexpr Canvas FourByThreeCanvas = {608, 456};
constexpr Canvas SixteenByNineCanvas = {832, 456};

// How wide the part of the raster with the menu is. The menu picks the width from the screen shape, and the part lies in the middle.
constexpr float FourByThreePictureWidth = 670;
constexpr float SixteenByNinePictureWidth = 686;

// The part starts 12 lines below the top of the raster.
constexpr float PictureTop = 12;

// The seven coefficients of the vertical filter of the copy, out of 64, as the menu sets them for each scan.
// The first two belong to the line above, the next three to the line itself and the last two to the line below.
constexpr std::size_t CoefficientCount = 7;
constexpr std::array<std::uint8_t, CoefficientCount> ProgressiveCoefficients = {0, 0, 21, 22, 21, 0, 0};
constexpr std::array<std::uint8_t, CoefficientCount> InterlacedCoefficients = {7, 7, 12, 12, 12, 7, 7};
constexpr double CoefficientTotal = 64;
constexpr double FullWeight = 255;

constexpr std::size_t FirstCoefficientOfLineAbove = 0;
constexpr std::size_t FirstCoefficientOfLineBelow = 5;
constexpr std::size_t CoefficientCountOfOutsideLine = 2;

// The three weights are the red, green and blue number of this constant color of the material.
constexpr std::size_t WeightColorIndex = 3;

float PictureWidthOf(Platform::ScreenShape screenShape)
{
    if (screenShape == Platform::ScreenShape::FourByThree)
        return FourByThreePictureWidth;

    return SixteenByNinePictureWidth;
}

int AddCoefficients(const std::array<std::uint8_t, CoefficientCount>& coefficients, std::size_t firstCoefficient, std::size_t count)
{
    int total = 0;
    for (std::size_t coefficientNumber = firstCoefficient; coefficientNumber < firstCoefficient + count; ++coefficientNumber)
        total += coefficients[coefficientNumber];
    return total;
}

std::uint8_t ToWeight(int coefficientTotal)
{
    return static_cast<std::uint8_t>(std::lround(coefficientTotal * FullWeight / CoefficientTotal));
}

// Multiplies its texture with its weight and adds it to what the stages before it made. The alpha is one in every stage.
TevStage MakeWeightedStage(std::uint8_t textureIndex, TevConstant weightChannel)
{
    TevStage stage;
    stage.textureIndex = textureIndex;
    stage.usesRasterColor = false;
    stage.constantColor = weightChannel;
    stage.constantAlpha = TevConstant::EightEighths;
    stage.color.inputB = TevColorInput::TextureColor;
    stage.color.inputC = TevColorInput::ConstantColor;
    stage.alpha.inputD = TevAlphaInput::ConstantAlpha;

    bool isFirstStage = textureIndex == 0;
    if (!isFirstStage)
        stage.color.inputD = TevColorInput::PreviousColor;

    return stage;
}

void AddWeightedLine(Material& material, TextureHandle frameBuffer, int lineOffset, TevConstant weightChannel)
{
    TextureMatrix matrix;
    matrix.rows[1][2] = static_cast<float>(lineOffset) / ConsoleFrameHeight;

    std::uint8_t textureIndex = static_cast<std::uint8_t>(material.textures.size());
    material.textures.push_back({frameBuffer, matrix});
    material.stages.push_back(MakeWeightedStage(textureIndex, weightChannel));
}

}

Canvas ConsoleCanvas(Platform::ScreenShape screenShape)
{
    if (screenShape == Platform::ScreenShape::FourByThree)
        return FourByThreeCanvas;

    return SixteenByNineCanvas;
}

Platform::PixelRectangle ConsolePictureArea(const Platform::PixelRectangle& screenArea, Platform::ScreenShape screenShape)
{
    float pictureWidth = PictureWidthOf(screenShape);
    float pictureLeft = (ConsoleRasterCanvas.width - pictureWidth) / 2;

    float pixelsPerRasterColumn = screenArea.width / ConsoleRasterCanvas.width;
    float pixelsPerRasterLine = screenArea.height / ConsoleRasterCanvas.height;

    return {screenArea.left + pictureLeft * pixelsPerRasterColumn, screenArea.top + PictureTop * pixelsPerRasterLine,
        pictureWidth * pixelsPerRasterColumn, ConsoleFrameHeight * pixelsPerRasterLine};
}

Quad MakeConsolePictureQuad(Platform::ScreenShape screenShape)
{
    float halfWidth = PictureWidthOf(screenShape) / 2;
    float halfHeight = ConsoleFrameHeight / 2.0f;

    constexpr Formats::Rgba8Color white = {255, 255, 255, 255};
    constexpr std::array<Formats::Vector2, QuadCornerCount> wholeTexture = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};

    Quad quad = {};
    quad.positions = {{{-halfWidth, halfHeight}, {halfWidth, halfHeight}, {halfWidth, -halfHeight}, {-halfWidth, -halfHeight}}};
    quad.colors = {{white, white, white, white}};
    quad.textureCoordinates = {wholeTexture, wholeTexture, wholeTexture};
    return quad;
}

ConsoleFilterWeights FindConsoleFilterWeights(ConsoleScan consoleScan)
{
    bool isProgressive = consoleScan == ConsoleScan::Progressive;
    const std::array<std::uint8_t, CoefficientCount>& coefficients = isProgressive ? ProgressiveCoefficients : InterlacedCoefficients;

    ConsoleFilterWeights weights;
    weights.lineAbove = ToWeight(AddCoefficients(coefficients, FirstCoefficientOfLineAbove, CoefficientCountOfOutsideLine));
    weights.lineBelow = ToWeight(AddCoefficients(coefficients, FirstCoefficientOfLineBelow, CoefficientCountOfOutsideLine));

    // The line itself takes what the rounding of the two others left, so the three always add up to 255.
    weights.lineItself = static_cast<std::uint8_t>(FullWeight - weights.lineAbove - weights.lineBelow);
    return weights;
}

Material MakeConsoleFilterMaterial(TextureHandle frameBuffer, ConsoleScan consoleScan)
{
    ConsoleFilterWeights weights = FindConsoleFilterWeights(consoleScan);

    Material material;
    material.blendMode = BlendMode::Replace;
    material.constantColors[WeightColorIndex] = {weights.lineAbove, weights.lineItself, weights.lineBelow, 255};

    AddWeightedLine(material, frameBuffer, -1, TevConstant::Color3Red);
    AddWeightedLine(material, frameBuffer, 0, TevConstant::Color3Green);
    AddWeightedLine(material, frameBuffer, 1, TevConstant::Color3Blue);
    return material;
}

}
