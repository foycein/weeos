// The data of the draws. The C++ side writes it into two buffers; these structures must stay the same as
// the ones in Graphics/DrawData.h.

// The same numbers as in Graphics/GraphicsDevice.h.
const uint CornersPerQuad = 4u;
const uint MaximumTextureCount = 3u;
const uint ColorRegisterCount = 3u;
const uint ConstantColorCount = 4u;

const uint NumbersPerTextureMatrix = 6u;

struct Stage
{
    uint textureIndex;
    uint usesRasterColor;
    uint constantColor;
    uint constantAlpha;

    uint colorInputA;
    uint colorInputB;
    uint colorInputC;
    uint colorInputD;
    uint colorOperation;
    uint colorBias;
    uint colorScale;
    uint colorClamp;
    uint colorOutput;

    uint alphaInputA;
    uint alphaInputB;
    uint alphaInputC;
    uint alphaInputD;
    uint alphaOperation;
    uint alphaBias;
    uint alphaScale;
    uint alphaClamp;
    uint alphaOutput;
};

struct DrawRecord
{
    vec4 colorRegisters[ColorRegisterCount];
    vec4 constantColors[ConstantColorCount];
    vec2 cornerPositions[CornersPerQuad];

    // Texture number n, corner c is at n * CornersPerQuad + c.
    vec2 textureCoordinates[MaximumTextureCount * CornersPerQuad];

    // Texture number n has its six numbers at n * NumbersPerTextureMatrix.
    float textureMatrices[MaximumTextureCount * NumbersPerTextureMatrix];

    uint cornerColors[CornersPerQuad];
    uint firstStage;
    uint stageCount;
};

layout(std430, set = 0, binding = 0) readonly buffer DrawRecords
{
    DrawRecord drawRecords[];
};

layout(std430, set = 0, binding = 1) readonly buffer Stages
{
    Stage stages[];
};
