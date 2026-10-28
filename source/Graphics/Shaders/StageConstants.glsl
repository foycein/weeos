// The numbers of the enumerations in Graphics/TevStage.h, with the same names.

const uint NoTexture = 255u;

const uint ColorInputPreviousColor = 0u;
const uint ColorInputPreviousAlpha = 1u;
const uint ColorInputRegister0Color = 2u;
const uint ColorInputRegister0Alpha = 3u;
const uint ColorInputRegister1Color = 4u;
const uint ColorInputRegister1Alpha = 5u;
const uint ColorInputRegister2Color = 6u;
const uint ColorInputRegister2Alpha = 7u;
const uint ColorInputTextureColor = 8u;
const uint ColorInputTextureAlpha = 9u;
const uint ColorInputRasterColor = 10u;
const uint ColorInputRasterAlpha = 11u;
const uint ColorInputOne = 12u;
const uint ColorInputHalf = 13u;
const uint ColorInputConstantColor = 14u;
const uint ColorInputZero = 15u;

const uint AlphaInputPreviousAlpha = 0u;
const uint AlphaInputRegister0Alpha = 1u;
const uint AlphaInputRegister1Alpha = 2u;
const uint AlphaInputRegister2Alpha = 3u;
const uint AlphaInputTextureAlpha = 4u;
const uint AlphaInputRasterAlpha = 5u;
const uint AlphaInputConstantAlpha = 6u;
const uint AlphaInputZero = 7u;

const uint OperationAdd = 0u;
const uint OperationSubtract = 1u;
const uint OperationRedGreater = 8u;
const uint OperationRedEqual = 9u;
const uint OperationGreenRedGreater = 10u;
const uint OperationGreenRedEqual = 11u;
const uint OperationBlueGreenRedGreater = 12u;
const uint OperationBlueGreenRedEqual = 13u;
const uint OperationEachGreater = 14u;
const uint OperationEachEqual = 15u;

const uint BiasAddHalf = 1u;
const uint BiasSubtractHalf = 2u;

const uint ScaleTwo = 1u;
const uint ScaleFour = 2u;
const uint ScaleHalf = 3u;

// The constants 0 to 7 are the fractions eight eighths down to one eighth.
// 12 to 15 are the four constant colors, and from 16 on come the single channels: four reds, four greens, four blues, four alphas.
const uint LastFractionConstant = 7u;
const uint FirstWholeColorConstant = 12u;
const uint FirstSingleChannelConstant = 16u;
