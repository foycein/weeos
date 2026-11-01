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
    uint swaps;
    uint isMovedByIndirectTexture;
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

    uint skipsTransparentPixels;

    // For a mesh: its number in the mesh buffer.
    uint meshNumber;

    // The indirect texture of the material and the two rows of its matrix.
    uint indirectTextureIndex;
    uint alphaTest;
    float indirectMatrix[NumbersPerTextureMatrix];
    uint edgeSharpening;
    uint unusedAtTheEnd;
};

// One mesh: the rows of its matrices and its light.
struct MeshRecord
{
    vec4 modelView[3];
    vec4 normalMatrix[3];
    vec4 projection[4];
    vec4 lightPosition;
    vec4 lightColor;
    vec4 ambientColor;
    uint hasLight;
    uint textureCoordinatesFromNormal;
    uint usesStoredCorners;
    uint firstBone;
    uint boneCount;
    uint usesCornerColors;
    uint unused[2];
};

struct Bone
{
    vec4 pointMatrix[3];
    vec4 normalMatrix[3];
};

struct MeshCorner
{
    vec4 positionAndS;
    vec4 normalAndT;
    uvec4 boneNumberAndColor;
};

layout(std430, set = 0, binding = 0) readonly buffer DrawRecords
{
    DrawRecord drawRecords[];
};

layout(std430, set = 0, binding = 1) readonly buffer Stages
{
    Stage stages[];
};

layout(std430, set = 0, binding = 2) readonly buffer MeshRecords
{
    MeshRecord meshRecords[];
};

layout(std430, set = 0, binding = 3) readonly buffer MeshCorners
{
    MeshCorner meshCorners[];
};

layout(std430, set = 0, binding = 4) readonly buffer StoredMeshCorners
{
    MeshCorner storedMeshCorners[];
};

layout(std430, set = 0, binding = 5) readonly buffer Bones
{
    Bone bones[];
};
