#pragma once

#include <cstdint>

namespace Graphics
{

// A stage combines colors, one step of the color calculation of the Wii's graphics chip (a TEV stage).
// A material lists up to 16 stages. They run in order, each one reading the results of the stages before it.
//
// Every stage calculates the color and the alpha separately, each with its four inputs A, B, C and D:
//     Add:       result = (D + (1 - C) * A + C * B + bias) * scale
//     Subtract:  result = (D - ((1 - C) * A + C * B) + bias) * scale
// and then keeps the result in an output register, clamped to the range of black to white when the clamp is on.
//
// The numbers of the enumerations are the numbers the layout files store, so the layout reader can convert them directly.

enum class TevColorInput : std::uint8_t
{
    PreviousColor = 0,
    PreviousAlpha = 1,
    Register0Color = 2,
    Register0Alpha = 3,
    Register1Color = 4,
    Register1Alpha = 5,
    Register2Color = 6,
    Register2Alpha = 7,
    TextureColor = 8,
    TextureAlpha = 9,
    RasterColor = 10,
    RasterAlpha = 11,
    One = 12,
    Half = 13,
    ConstantColor = 14,
    Zero = 15,
};

enum class TevAlphaInput : std::uint8_t
{
    PreviousAlpha = 0,
    Register0Alpha = 1,
    Register1Alpha = 2,
    Register2Alpha = 3,
    TextureAlpha = 4,
    RasterAlpha = 5,
    ConstantAlpha = 6,
    Zero = 7,
};

// The compare operations give D + (A is greater than B, or equal to B ? C : 0).
// What is compared depends on the operation. In an alpha stage only Add, Subtract, EachGreater and EachEqual exist,
// and the last two compare the alpha values of A and B.
enum class TevOperation : std::uint8_t
{
    Add = 0,
    Subtract = 1,
    RedGreater = 8,
    RedEqual = 9,
    GreenRedGreater = 10,
    GreenRedEqual = 11,
    BlueGreenRedGreater = 12,
    BlueGreenRedEqual = 13,
    EachGreater = 14,
    EachEqual = 15,
};

enum class TevBias : std::uint8_t
{
    Zero = 0,
    AddHalf = 1,
    SubtractHalf = 2,
};

enum class TevScale : std::uint8_t
{
    One = 0,
    Two = 1,
    Four = 2,
    Half = 3,
};

// Where the result of a stage is kept. Previous is what the next stage sees as "previous"
// and what the picture shows after the last stage.
enum class TevRegister : std::uint8_t
{
    Previous = 0,
    Register0 = 1,
    Register1 = 2,
    Register2 = 3,
};

// What a stage reads when an input is the constant color or the constant alpha.
// The numbers 0 to 7 are fractions, the others are the four constant colors of the material or one channel of them.
// The constant alpha of a stage must be a fraction or a channel, not a whole color.
enum class TevConstant : std::uint8_t
{
    EightEighths = 0,
    SevenEighths = 1,
    SixEighths = 2,
    FiveEighths = 3,
    FourEighths = 4,
    ThreeEighths = 5,
    TwoEighths = 6,
    OneEighth = 7,

    Color0 = 12,
    Color1 = 13,
    Color2 = 14,
    Color3 = 15,

    Color0Red = 16,
    Color1Red = 17,
    Color2Red = 18,
    Color3Red = 19,

    Color0Green = 20,
    Color1Green = 21,
    Color2Green = 22,
    Color3Green = 23,

    Color0Blue = 24,
    Color1Blue = 25,
    Color2Blue = 26,
    Color3Blue = 27,

    Color0Alpha = 28,
    Color1Alpha = 29,
    Color2Alpha = 30,
    Color3Alpha = 31,
};

struct TevColorCalculation
{
    TevColorInput inputA = TevColorInput::Zero;
    TevColorInput inputB = TevColorInput::Zero;
    TevColorInput inputC = TevColorInput::Zero;
    TevColorInput inputD = TevColorInput::Zero;
    TevOperation operation = TevOperation::Add;
    TevBias bias = TevBias::Zero;
    TevScale scale = TevScale::One;
    bool clamp = true;
    TevRegister output = TevRegister::Previous;
};

struct TevAlphaCalculation
{
    TevAlphaInput inputA = TevAlphaInput::Zero;
    TevAlphaInput inputB = TevAlphaInput::Zero;
    TevAlphaInput inputC = TevAlphaInput::Zero;
    TevAlphaInput inputD = TevAlphaInput::Zero;
    TevOperation operation = TevOperation::Add;
    TevBias bias = TevBias::Zero;
    TevScale scale = TevScale::One;
    bool clamp = true;
    TevRegister output = TevRegister::Previous;
};

// The texture number of a stage that reads no texture.
constexpr std::uint8_t NoTexture = 0xFF;

struct TevStage
{
    // The number of the texture of the material (0 to 2) that the texture inputs read, or NoTexture.
    // The texture is read with the texture coordinates of the same number.
    std::uint8_t textureIndex = NoTexture;

    // The raster color is the color of the corners of the quad. When this is false, the raster inputs read zero.
    bool usesRasterColor = true;

    TevConstant constantColor = TevConstant::Color0;
    TevConstant constantAlpha = TevConstant::Color0Alpha;

    TevColorCalculation color;
    TevAlphaCalculation alpha;
};

}
