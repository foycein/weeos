// The data of the draws. The C++ side writes it into two buffers; these structures must stay the same as
// the ones in Graphics/DrawData.h.

const uint CornersPerQuad = 4u;
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
    vec4 colorRegisters[3];
    vec4 constantColors[4];
    vec2 cornerPositions[4];

    // Texture number n, corner c is at n * 4 + c.
    vec2 textureCoordinates[12];

    // Texture number n has its six numbers at n * 6.
    float textureMatrices[18];

    uint cornerColors[4];
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
