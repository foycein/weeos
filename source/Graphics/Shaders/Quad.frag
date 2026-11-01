#version 450
#extension GL_GOOGLE_include_directive : require

#include "DrawData.glsl"
#include "StageConstants.glsl"

// The textures of the material. Texture n of the material is read with the texture coordinates n.
layout(set = 1, binding = 0) uniform sampler2D firstTexture;
layout(set = 2, binding = 0) uniform sampler2D secondTexture;
layout(set = 3, binding = 0) uniform sampler2D thirdTexture;

layout(location = 0) in vec4 rasterColor;
layout(location = 1) in vec2 firstTextureCoordinates;
layout(location = 2) in vec2 secondTextureCoordinates;
layout(location = 3) in vec2 thirdTextureCoordinates;
layout(location = 4) flat in uint drawIndex;

layout(location = 0) out vec4 pictureColor;

// This program runs the stages of the material one after the other, the way the color calculation of the Wii does.
// Every stage calculates its color and its alpha with four inputs A, B, C and D:
//     Add:       result = (D + (1 - C) * A + C * B + bias) * scale
//     Subtract:  result = (D - ((1 - C) * A + C * B) + bias) * scale
// The compare operations give D + (A is greater than B, or equal to B ? C : 0) and ignore bias and scale.
// The inputs A, B and C are limited to the range 0 to 1 and D can be any value (assumed, see SaturateInput below).

// The registers a stage can read and write. Number 0 is the previous result, 1 to 3 are REG0 to REG2.
// They hold the colors in the range 0 to 1, where 1 is the 255 of the material, and they can go below 0 and above 1.
vec4 registers[4];

const float MaximumByteValue = 255.0;
const uint ValuesPerByte = 256u;
const float LowestRegisterValue = -1024.0 / MaximumByteValue;
const float HighestRegisterValue = 1023.0 / MaximumByteValue;

const uint EighthsInOne = 8u;

// An alpha below half a step of 1/255 is the alpha 0 of the Wii.
const float LargestZeroAlpha = 0.5 / MaximumByteValue;

// Assumed (docs/audit/OpenPoints/Graphics.md): the inputs A, B and C of a stage are limited to black and white, input D is not.
// The registers hold more than that, and no public document says what the Wii does with such a value in A, B or C.
const float LowestSaturatedInput = 0.0;
const float HighestSaturatedInput = 1.0;

vec3 SaturateInput(vec3 value)
{
    return clamp(value, LowestSaturatedInput, HighestSaturatedInput);
}

float SaturateInput(float value)
{
    return clamp(value, LowestSaturatedInput, HighestSaturatedInput);
}

// What a stage can read besides the registers.
struct StageSources
{
    vec4 textureSample;
    vec4 rasterSample;
    vec3 constantColor;
    float constantAlpha;
};

// The pictures have no smaller copies, so the sample always comes from the one picture there is. The shift is in texture coordinates.
vec4 SampleTexture(uint textureNumber, vec2 shift)
{
    switch (textureNumber)
    {
    case 0u:
        return textureLod(firstTexture, firstTextureCoordinates + shift, 0.0);
    case 1u:
        return textureLod(secondTexture, secondTextureCoordinates + shift, 0.0);
    case 2u:
        return textureLod(thirdTexture, thirdTextureCoordinates + shift, 0.0);
    }
    return vec4(0.0);
}

vec4 SampleTexture(uint textureNumber)
{
    return SampleTexture(textureNumber, vec2(0.0));
}

vec2 TextureSize(uint textureNumber)
{
    switch (textureNumber)
    {
    case 0u:
        return vec2(textureSize(firstTexture, 0));
    case 1u:
        return vec2(textureSize(secondTexture, 0));
    case 2u:
        return vec2(textureSize(thirdTexture, 0));
    }
    return vec2(1.0);
}

// The indirect texel gives three numbers from -128 to 127 (its alpha, blue and green), and the matrix turns them into texels.
const float IndirectBias = 128.0;

vec2 IndirectShift(uint textureNumber)
{
    vec4 texel = SampleTexture(drawRecords[drawIndex].indirectTextureIndex);
    vec3 numbers = vec3(texel.a, texel.b, texel.g) * MaximumByteValue - IndirectBias;
    float matrix[6] = drawRecords[drawIndex].indirectMatrix;
    float shiftS = matrix[0] * numbers.x + matrix[1] * numbers.y + matrix[2] * numbers.z;
    float shiftT = matrix[3] * numbers.x + matrix[4] * numbers.y + matrix[5] * numbers.z;
    return vec2(shiftS, shiftT) / TextureSize(textureNumber);
}

vec4 SampleTextureOfStage(Stage stage)
{
    if (stage.textureIndex == NoTexture)
        return vec4(0.0);

    if (stage.isMovedByIndirectTexture == 0u)
        return SampleTexture(stage.textureIndex);

    return SampleTexture(stage.textureIndex, IndirectShift(stage.textureIndex));
}

float ConstantFraction(uint constantNumber)
{
    return float(EighthsInOne - constantNumber) / float(EighthsInOne);
}

float ConstantChannel(uint constantNumber)
{
    uint channelOrder = constantNumber - FirstSingleChannelConstant;
    uint colorNumber = channelOrder % ConstantColorCount;
    uint channelNumber = channelOrder / ConstantColorCount;
    return drawRecords[drawIndex].constantColors[colorNumber][channelNumber];
}

vec3 ConstantColorValue(uint constantNumber)
{
    if (constantNumber <= LastFractionConstant)
        return vec3(ConstantFraction(constantNumber));

    if (constantNumber < FirstSingleChannelConstant)
        return drawRecords[drawIndex].constantColors[constantNumber - FirstWholeColorConstant].rgb;

    return vec3(ConstantChannel(constantNumber));
}

float ConstantAlphaValue(uint constantNumber)
{
    if (constantNumber <= LastFractionConstant)
        return ConstantFraction(constantNumber);

    return ConstantChannel(constantNumber);
}

vec3 ColorInputValue(uint inputNumber, StageSources sources)
{
    switch (inputNumber)
    {
    case ColorInputPreviousColor:
        return registers[0].rgb;
    case ColorInputPreviousAlpha:
        return vec3(registers[0].a);
    case ColorInputRegister0Color:
        return registers[1].rgb;
    case ColorInputRegister0Alpha:
        return vec3(registers[1].a);
    case ColorInputRegister1Color:
        return registers[2].rgb;
    case ColorInputRegister1Alpha:
        return vec3(registers[2].a);
    case ColorInputRegister2Color:
        return registers[3].rgb;
    case ColorInputRegister2Alpha:
        return vec3(registers[3].a);
    case ColorInputTextureColor:
        return sources.textureSample.rgb;
    case ColorInputTextureAlpha:
        return vec3(sources.textureSample.a);
    case ColorInputRasterColor:
        return sources.rasterSample.rgb;
    case ColorInputRasterAlpha:
        return vec3(sources.rasterSample.a);
    case ColorInputOne:
        return vec3(1.0);
    case ColorInputHalf:
        return vec3(0.5);
    case ColorInputConstantColor:
        return sources.constantColor;
    }
    return vec3(0.0);
}

float AlphaInputValue(uint inputNumber, StageSources sources)
{
    switch (inputNumber)
    {
    case AlphaInputPreviousAlpha:
        return registers[0].a;
    case AlphaInputRegister0Alpha:
        return registers[1].a;
    case AlphaInputRegister1Alpha:
        return registers[2].a;
    case AlphaInputRegister2Alpha:
        return registers[3].a;
    case AlphaInputTextureAlpha:
        return sources.textureSample.a;
    case AlphaInputRasterAlpha:
        return sources.rasterSample.a;
    case AlphaInputConstantAlpha:
        return sources.constantAlpha;
    }
    return 0.0;
}

float BiasValue(uint biasNumber)
{
    if (biasNumber == BiasAddHalf)
        return 0.5;

    if (biasNumber == BiasSubtractHalf)
        return -0.5;

    return 0.0;
}

float ScaleValue(uint scaleNumber)
{
    switch (scaleNumber)
    {
    case ScaleTwo:
        return 2.0;
    case ScaleFour:
        return 4.0;
    case ScaleHalf:
        return 0.5;
    }
    return 1.0;
}

// The compare operations look at the colors as whole numbers from 0 to 255.
uvec3 ColorBytes(vec3 color)
{
    return uvec3(round(clamp(color, 0.0, 1.0) * MaximumByteValue));
}

uint AlphaByte(float alpha)
{
    return uint(round(clamp(alpha, 0.0, 1.0) * MaximumByteValue));
}

uint GreenRedNumber(uvec3 bytes)
{
    return bytes.g * ValuesPerByte + bytes.r;
}

uint BlueGreenRedNumber(uvec3 bytes)
{
    return (bytes.b * ValuesPerByte + bytes.g) * ValuesPerByte + bytes.r;
}

vec3 CompareColors(uint operation, vec3 a, vec3 b, vec3 c, vec3 d)
{
    uvec3 aBytes = ColorBytes(a);
    uvec3 bBytes = ColorBytes(b);

    switch (operation)
    {
    case OperationRedGreater:
        return d + (aBytes.r > bBytes.r ? c : vec3(0.0));
    case OperationRedEqual:
        return d + (aBytes.r == bBytes.r ? c : vec3(0.0));
    case OperationGreenRedGreater:
        return d + (GreenRedNumber(aBytes) > GreenRedNumber(bBytes) ? c : vec3(0.0));
    case OperationGreenRedEqual:
        return d + (GreenRedNumber(aBytes) == GreenRedNumber(bBytes) ? c : vec3(0.0));
    case OperationBlueGreenRedGreater:
        return d + (BlueGreenRedNumber(aBytes) > BlueGreenRedNumber(bBytes) ? c : vec3(0.0));
    case OperationBlueGreenRedEqual:
        return d + (BlueGreenRedNumber(aBytes) == BlueGreenRedNumber(bBytes) ? c : vec3(0.0));
    case OperationEachGreater:
        return d + mix(vec3(0.0), c, greaterThan(aBytes, bBytes));
    }
    return d + mix(vec3(0.0), c, equal(aBytes, bBytes));
}

float CompareAlphas(uint operation, float a, float b, float c, float d)
{
    if (operation == OperationEachGreater)
        return d + (AlphaByte(a) > AlphaByte(b) ? c : 0.0);

    return d + (AlphaByte(a) == AlphaByte(b) ? c : 0.0);
}

vec3 CalculateColor(Stage stage, StageSources sources)
{
    vec3 a = SaturateInput(ColorInputValue(stage.colorInputA, sources));
    vec3 b = SaturateInput(ColorInputValue(stage.colorInputB, sources));
    vec3 c = SaturateInput(ColorInputValue(stage.colorInputC, sources));
    vec3 d = ColorInputValue(stage.colorInputD, sources);

    vec3 result;
    if (stage.colorOperation == OperationAdd)
        result = (d + (1.0 - c) * a + c * b + BiasValue(stage.colorBias)) * ScaleValue(stage.colorScale);
    else if (stage.colorOperation == OperationSubtract)
        result = (d - ((1.0 - c) * a + c * b) + BiasValue(stage.colorBias)) * ScaleValue(stage.colorScale);
    else
        result = CompareColors(stage.colorOperation, a, b, c, d);

    if (stage.colorClamp != 0u)
        return clamp(result, 0.0, 1.0);

    return clamp(result, LowestRegisterValue, HighestRegisterValue);
}

float CalculateAlpha(Stage stage, StageSources sources)
{
    float a = SaturateInput(AlphaInputValue(stage.alphaInputA, sources));
    float b = SaturateInput(AlphaInputValue(stage.alphaInputB, sources));
    float c = SaturateInput(AlphaInputValue(stage.alphaInputC, sources));
    float d = AlphaInputValue(stage.alphaInputD, sources);

    float result;
    if (stage.alphaOperation == OperationAdd)
        result = (d + (1.0 - c) * a + c * b + BiasValue(stage.alphaBias)) * ScaleValue(stage.alphaScale);
    else if (stage.alphaOperation == OperationSubtract)
        result = (d - ((1.0 - c) * a + c * b) + BiasValue(stage.alphaBias)) * ScaleValue(stage.alphaScale);
    else
        result = CompareAlphas(stage.alphaOperation, a, b, c, d);

    if (stage.alphaClamp != 0u)
        return clamp(result, 0.0, 1.0);

    return clamp(result, LowestRegisterValue, HighestRegisterValue);
}

// The swap table of a stage: every channel reads the channel whose number its two bits hold.
const uint TextureSwapShift = 8u;

vec4 Swapped(vec4 color, uint table)
{
    return vec4(color[table & 3u], color[(table >> 2u) & 3u], color[(table >> 4u) & 3u], color[(table >> 6u) & 3u]);
}

void RunStage(Stage stage)
{
    StageSources sources;
    sources.textureSample = SampleTextureOfStage(stage);
    sources.rasterSample = stage.usesRasterColor != 0u ? rasterColor : vec4(0.0);
    sources.textureSample = Swapped(sources.textureSample, stage.swaps >> TextureSwapShift);
    sources.rasterSample = Swapped(sources.rasterSample, stage.swaps);
    sources.constantColor = ConstantColorValue(stage.constantColor);
    sources.constantAlpha = ConstantAlphaValue(stage.constantAlpha);

    // Both calculations read the registers as they were before the stage.
    vec3 color = CalculateColor(stage, sources);
    float alpha = CalculateAlpha(stage, sources);

    registers[stage.colorOutput].rgb = color;
    registers[stage.alphaOutput].a = alpha;
}

// The alpha test of the material (PackAlphaTest in DrawData.cpp).
const uint AlphaTestIsOn = 0x80000000u;
const uint ComparisonMask = 7u;
const uint JoinMask = 3u;
const uint ByteMask = 255u;
const uint SecondComparisonShift = 3u;
const uint JoinShift = 6u;
const uint FirstReferenceShift = 8u;
const uint SecondReferenceShift = 16u;

const uint CompareNever = 0u;
const uint CompareLess = 1u;
const uint CompareEqual = 2u;
const uint CompareLessOrEqual = 3u;
const uint CompareGreater = 4u;
const uint CompareNotEqual = 5u;
const uint CompareGreaterOrEqual = 6u;

const uint JoinAnd = 0u;
const uint JoinOr = 1u;
const uint JoinExclusiveOr = 2u;

bool PassesComparison(uint alpha, uint comparison, uint reference)
{
    if (comparison == CompareNever)
        return false;
    if (comparison == CompareLess)
        return alpha < reference;
    if (comparison == CompareEqual)
        return alpha == reference;
    if (comparison == CompareLessOrEqual)
        return alpha <= reference;
    if (comparison == CompareGreater)
        return alpha > reference;
    if (comparison == CompareNotEqual)
        return alpha != reference;
    if (comparison == CompareGreaterOrEqual)
        return alpha >= reference;
    return true;
}

bool PassesAlphaTest(float alphaValue, uint test)
{
    if ((test & AlphaTestIsOn) == 0u)
        return true;

    uint alpha = uint(round(alphaValue * MaximumByteValue));
    bool first = PassesComparison(alpha, test & ComparisonMask, (test >> FirstReferenceShift) & ByteMask);
    bool second = PassesComparison(alpha, (test >> SecondComparisonShift) & ComparisonMask, (test >> SecondReferenceShift) & ByteMask);

    uint join = (test >> JoinShift) & JoinMask;
    if (join == JoinAnd)
        return first && second;
    if (join == JoinOr)
        return first || second;
    if (join == JoinExclusiveOr)
        return first != second;
    return first == second;
}

void main()
{
    registers[0] = vec4(0.0);
    for (uint registerNumber = 0u; registerNumber < ColorRegisterCount; ++registerNumber)
        registers[registerNumber + 1u] = drawRecords[drawIndex].colorRegisters[registerNumber];

    uint firstStage = drawRecords[drawIndex].firstStage;
    uint stageCount = drawRecords[drawIndex].stageCount;
    for (uint stageNumber = 0u; stageNumber < stageCount; ++stageNumber)
        RunStage(stages[firstStage + stageNumber]);

    pictureColor = clamp(registers[0], 0.0, 1.0);

    bool isTransparent = pictureColor.a < LargestZeroAlpha;
    if (drawRecords[drawIndex].skipsTransparentPixels != 0u && isTransparent)
        discard;

    if (!PassesAlphaTest(pictureColor.a, drawRecords[drawIndex].alphaTest))
        discard;
}
