#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Formats
{

// The particle effects of the NintendoWare effect library (EF): a .breff file holds the emitters, a .breft file their pictures.
// The Forecast, News and Mii Channel files are version 5, the Wii Speak Channel's are version 11. Both are read into the same
// description; where the versions store a thing differently, the reader turns it into the same field.

constexpr std::uint16_t EffectFileVersion5 = 5;
constexpr std::uint16_t EffectFileVersion11 = 11;

constexpr std::size_t ParticleColorCount = 4;
constexpr std::size_t ParticleTextureCount = 3;
constexpr std::size_t EffectStageCount = 4;
constexpr std::size_t FormParameterCount = 6;

// The place where an emitter puts its new particles.
enum class EmitterForm : std::uint8_t
{
    Disc = 0,
    Line = 1,
    Cube = 5,
    Cylinder = 7,
    Sphere = 8,
    Point = 9,
    Torus = 10,
};

// The bits of the emitter's common flags.
namespace EmitterFlag
{
constexpr std::uint32_t Endless = 0x4;
constexpr std::uint32_t ParticlesInheritScale = 0x20;
constexpr std::uint32_t ParticlesInheritRotation = 0x40;
constexpr std::uint32_t ChildrenInheritScale = 0x80;
constexpr std::uint32_t ChildrenInheritRotation = 0x100;
constexpr std::uint32_t Paused = 0x200;
}

// The bits of the emit flags above the form in the lowest byte.
namespace EmitFlag
{
constexpr std::uint32_t FormMask = 0xFF;
constexpr std::uint32_t LevelOfDetail = 0x100;
constexpr std::uint32_t FixedCount = 0x20000;
constexpr std::uint32_t FixedStartAngle = 0x40000;
constexpr std::uint32_t EvenOverTheArea = 0x1000000;
constexpr std::uint32_t SameSizeOnEveryAxis = 0x2000000;
constexpr std::uint32_t LineCentered = 0x4000000;
}

// How the height of a particle is made from its size and scale.
enum class ParticleHeightRule : std::uint8_t
{
    SizeYTimesScaleY,
    SizeXTimesScaleY,
    SizeYTimesScaleX,
    SizeXTimesScaleX,
};

enum class ParticleDrawType : std::uint8_t
{
    Point = 0,
    Line = 1,
    Free = 2,
    Billboard = 3,
    Directional = 4,
    Stripe = 5,
    SmoothStripe = 6,
};

// The billboard's own kinds (ParticleShape::option).
namespace BillboardOption
{
constexpr std::uint8_t FacesTheViewer = 0;
constexpr std::uint8_t UprightOnY = 1;
constexpr std::uint8_t AlongTheDirection = 2;
}

// Where the direction of a directional particle or a stripe comes from (ParticleShape::direction).
namespace ParticleDirection
{
constexpr std::uint8_t Velocity = 0;
constexpr std::uint8_t FromTheEmitter = 1;
constexpr std::uint8_t EmitterYAxis = 2;
constexpr std::uint8_t ToTheNextParticle = 3;
constexpr std::uint8_t BetweenNeighbours = 4;
constexpr std::uint8_t AlongTheStripe = 6;
}

// The lowest three bits of ParticleShape::stripeFlags: how the end of a stripe is joined.
namespace StripeConnection
{
constexpr std::uint8_t Mask = 0x07;
constexpr std::uint8_t None = 0;
constexpr std::uint8_t BackToTheFirst = 1;
constexpr std::uint8_t ToTheEmitter = 2;
}

// Stripe flags bits 6 and 7 equal to this: the texture repeats once per segment instead of stretching over the whole stripe.
constexpr std::uint8_t StripeTexturePerSegmentMask = 0xC0;
constexpr std::uint8_t StripeTexturePerSegment = 0x40;

struct ParticleShape
{
    ParticleDrawType type = ParticleDrawType::Billboard;
    std::uint8_t option = 0;
    std::uint8_t direction = 0;
    std::uint8_t axis = 0;

    // Directional particles and billboards along a direction stretch with their speed when this is not zero.
    std::uint8_t stretchesWithSpeed = 0;

    // Smooth stripes: how many pieces each segment between two particles is cut into (0 counts as 1).
    std::uint8_t stripeSubdivisions = 0;
    std::uint8_t stripeFlags = 0;

    // Where the particle's own point lies on its picture, from -1 to 1 (the files give percent).
    Vector2 pivot = {0, 0};

    ParticleHeightRule heightRule = ParticleHeightRule::SizeYTimesScaleY;
};

// One stage of the graphics chip's color combiner, with the chip's own numbers (Graphics/TevStage.h names them).
struct EffectStage
{
    // Version 11 names the texture of each stage: 0 texture 1, 1 texture 2, anything else none. Version 5 reads texture 1 in the
    // first stage and texture 2 in the others; the reader gives those numbers here.
    std::uint8_t textureNumber = 0;

    std::array<std::uint8_t, 4> colorInputs = {};

    // Operation, bias, scale, clamp, output register.
    std::array<std::uint8_t, 5> colorOperation = {};
    std::array<std::uint8_t, 4> alphaInputs = {};
    std::array<std::uint8_t, 5> alphaOperation = {};
    std::uint8_t constantColorSelection = 0;
    std::uint8_t constantAlphaSelection = 0;
};

// Which of the particle's four colors fills a register of the stages.
enum class ParticleColorSource : std::uint8_t
{
    None = 0,
    Color1Primary = 1,
    Color1Secondary = 2,
    Color2Primary = 3,
    Color2Secondary = 4,
    Color1PrimaryTimesSecondary = 5,
    Color2PrimaryTimesSecondary = 6,
};

// The bits of EffectDrawSetting::flags.
namespace DrawFlag
{
constexpr std::uint16_t DepthTest = 0x1;
constexpr std::uint16_t DepthWrite = 0x2;
constexpr std::uint16_t UsesTexture1 = 0x10;
constexpr std::uint16_t UsesTexture2 = 0x20;
constexpr std::uint16_t UsesIndirectTexture = 0x40;
constexpr std::uint16_t OldestFirst = 0x800;
}

// How the particles of an emitter are colored and laid over the picture.
struct EffectDrawSetting
{
    std::uint16_t flags = 0;

    // The alpha test of the chip: two comparisons (GX_NEVER 0 to GX_ALWAYS 7) with the particle's two references, joined by an
    // operation (AND 0, OR 1, XOR 2, XNOR 3).
    std::uint8_t firstAlphaComparison = 7;
    std::uint8_t secondAlphaComparison = 7;
    std::uint8_t alphaComparisonJoin = 0;

    std::vector<EffectStage> stages;

    // The stages that read their texture through the indirect texture, one bit per stage.
    std::uint8_t indirectStageMask = 0;

    // The chip's blend: type (1 blend), source factor, destination factor.
    std::uint8_t blendType = 1;
    std::uint8_t blendSource = 4;
    std::uint8_t blendDestination = 5;

    std::array<ParticleColorSource, 3> registerColors = {};
    std::array<ParticleColorSource, 3> registerAlphas = {};
    std::array<ParticleColorSource, 4> constantColors = {};
    std::array<ParticleColorSource, 4> constantAlphas = {};

    // The light of the manager: 0 none, 1 the ambient color multiplies the particles.
    std::uint8_t lightingType = 0;
    Rgba8Color ambientColor = {255, 255, 255, 255};

    // Alpha flicker: 0 none.
    std::uint8_t flickerType = 0;

    std::array<std::array<float, 3>, 2> indirectMatrix = {};
    std::int8_t indirectScaleExponent = 0;
};

// The settings of the emitter itself.
struct EmitterSettings
{
    std::uint32_t commonFlags = 0;
    std::uint32_t emitFlags = 0;
    EmitterForm form = EmitterForm::Point;

    // In steps. EmitterFlag::Endless makes the emitter run until it is stopped.
    std::uint16_t emitterLife = 0;
    std::uint16_t particleLife = 0;
    std::int8_t particleLifeRandomPercent = 0;
    std::int8_t emitIntervalRandomPercent = 0;
    std::int8_t emitCountRandomPercent = 0;

    // Particles per emission; a fraction is kept for the next one.
    float emitCount = 0;
    std::uint16_t emitStartDelay = 0;

    // Steps that are run at once when the emitter is made.
    std::uint16_t preRollSteps = 0;
    std::uint16_t emitInterval = 0;

    // Sizes and angles of the form, in the order of each form.
    std::array<float, FormParameterCount> formParameters = {};

    // The number of particles of each emission with EmitFlag::FixedCount.
    std::uint16_t fixedCount = 0;

    std::uint8_t initialVelocityRandomPercent = 0;
    std::uint8_t momentumRandomPercent = 0;
    float radiationSpeed = 0;
    float yAxisSpeed = 0;
    float randomDirectionSpeed = 0;
    float normalSpeed = 0;
    float normalDiffusion = 0;
    float specialDirectionSpeed = 0;
    float specialDirectionDiffusion = 0;

    // Angles in radians that turn (0, 1, 0) into the special direction.
    Vector3 specialDirectionAngles = {0, 0, 0};

    Vector3 scale = {1, 1, 1};
    Vector3 rotation = {0, 0, 0};
    Vector3 translation = {0, 0, 0};

    // 0 takes a seed from the effect system.
    std::uint16_t randomSeed = 0;
};

struct ParticleTextureTransform
{
    Vector2 scale = {1, 1};
    float rotation = 0;
    Vector2 translation = {0, 0};
};

// What a new particle starts with.
struct ParticleParameters
{
    // Color 1 primary, color 1 secondary, color 2 primary, color 2 secondary.
    std::array<Rgba8Color, ParticleColorCount> colors = {};

    // Half the width and height of the particle's picture.
    Vector2 size = {0, 0};
    Vector2 scale = {1, 1};
    Vector3 rotation = {0, 0, 0};

    // Texture 1, texture 2 and the indirect texture.
    std::array<ParticleTextureTransform, ParticleTextureCount> textureTransforms = {};

    // Four bits per texture: horizontal wrap in bits 0 and 1, vertical in bits 2 and 3 (0 clamp, 1 repeat, 2 mirror).
    std::uint16_t textureWraps = 0;

    // Two bits per texture: 1 flips it horizontally, 2 vertically.
    std::uint8_t textureReverses = 0;

    std::uint8_t firstAlphaReference = 0;
    std::uint8_t secondAlphaReference = 0;

    // Version 5: the size is made up to this many percent larger or smaller.
    std::int8_t sizeRandomPercent = 0;

    // Version 11: a rotation added to every particle, and how much of it is random.
    Vector3 rotationOffset = {0, 0, 0};
    std::array<std::uint8_t, 3> rotationOffsetRandomPercent = {};

    // Empty where the emitter has no such texture.
    std::array<std::string, ParticleTextureCount> textureNames;
};

// What an animation changes.
enum class EffectAnimationTarget : std::uint8_t
{
    Color1PrimaryRgb,
    Color1PrimaryAlpha,
    Color1SecondaryRgb,
    Color1SecondaryAlpha,
    Color2PrimaryRgb,
    Color2PrimaryAlpha,
    Color2SecondaryRgb,
    Color2SecondaryAlpha,
    FirstAlphaReference,
    SecondAlphaReference,
    Size,
    Scale,
    Rotation,
    Texture1Pattern,
    Texture2Pattern,
    IndirectTexturePattern,
    Texture1Scale,
    Texture1Rotation,
    Texture1Translation,
    Texture2Scale,
    Texture2Rotation,
    Texture2Translation,
    IndirectTextureScale,
    IndirectTextureRotation,
    IndirectTextureTranslation,
    Child,
    GravityField,
    SpeedField,
    MagnetField,
    NewtonField,
    VortexField,
    SpinField,
    TailField,
    EmitterFormParameters,
    EmitterScale,
    EmitterRotation,
    EmitterTranslation,
    EmitterRadiationSpeed,
    EmitterYAxisSpeed,
    EmitterRandomDirectionSpeed,
    EmitterNormalSpeed,
    EmitterSpecialDirectionSpeed,
    EmitterEmitCount,
    EmitterUnnamed,
};

// The bits of EffectAnimation::flags.
namespace AnimationFlag
{
constexpr std::uint8_t OneRandomForAllComponents = 0x04;
constexpr std::uint8_t Disabled = 0x08;
constexpr std::uint8_t UsesEmitterTime = 0x10;
constexpr std::uint8_t LoopsForever = 0x20;
constexpr std::uint8_t MirrorsOddLoops = 0x40;
constexpr std::uint8_t FitsToLife = 0x80;
}

// The bits of AnimationKey::randomFlags.
namespace KeyRandomFlag
{
constexpr std::uint8_t FromRangeTable = 0x1;
constexpr std::uint8_t FromRandomTable = 0x2;
}

// What a child key asks for: particles or an emitter of the same effect, made at the place of the particle.
struct ChildRequest
{
    std::int16_t velocityPercent = 0;
    std::uint8_t sizePercent = 0;
    std::uint8_t alphaPercent = 0;
    std::uint8_t colorPercent = 0;
    std::uint8_t drawPriority = 0;
    bool makesEmitter = false;
    std::uint16_t nameIndex = 0;
};

struct AnimationKey
{
    std::uint16_t frame = 0;
    std::uint8_t interpolation = 0;
    std::uint8_t randomFlags = 0;

    // One value per component: floats, or the bytes of colors as 0 to 255.
    std::vector<float> values;

    // For a random key: the entry of the range table.
    std::uint16_t rangeIndex = 0;

    // Texture pattern keys.
    std::uint8_t textureWrap = 0;
    std::uint8_t textureReverse = 0;
    std::uint16_t textureNameIndex = 0;

    ChildRequest child;
};

// One entry of a range table: per component a base value and the step per unit of a 16-bit random number.
struct RandomRange
{
    std::vector<float> bases;
    std::vector<float> steps;

    // Rotations: a random bit turns all three angles negative.
    bool hasRandomSign = false;
};

// Extra numbers of a field: its value, direction, the space it works in and whether it pushes the position or the velocity.
struct FieldSettings
{
    std::vector<float> values;
    Vector3 direction = {0, 0, 0};

    // 0 world, 1 the emitter's own space.
    std::uint8_t space = 0;

    // 1 moves the position, 0 changes the velocity.
    std::uint8_t mode = 0;
};

enum class AnimationValueKind : std::uint8_t
{
    Bytes,
    Floats,
    Rotation,
    TexturePattern,
    Child,
    Field,
};

struct EffectAnimation
{
    EffectAnimationTarget target = EffectAnimationTarget::Size;
    AnimationValueKind kind = AnimationValueKind::Floats;
    std::size_t componentCount = 1;
    std::uint8_t flags = 0;
    std::uint8_t loopCount = 0;
    std::uint16_t randomSeed = 0;

    // The curve covers the frames 0 to frameCount - 1.
    std::uint16_t frameCount = 0;

    // Version 11 stores some curves as one value per frame instead of keys: frameCount * componentCount numbers.
    bool isBaked = false;
    std::vector<float> bakedValues;

    std::vector<AnimationKey> keys;
    std::vector<RandomRange> rangeTable;
    std::vector<RandomRange> randomTable;
    std::vector<std::string> names;
    FieldSettings field;
};

struct EmitterResource
{
    std::string name;
    std::uint16_t version = EffectFileVersion5;
    EmitterSettings emitter;
    ParticleShape shape;
    EffectDrawSetting drawSetting;
    ParticleParameters particle;

    // The first particleAnimationsAtBirth animations run only on the first step of a particle, the others on every step.
    std::vector<EffectAnimation> particleAnimations;
    std::size_t particleAnimationsAtBirth = 0;
    std::vector<EffectAnimation> emitterAnimations;
    std::size_t emitterAnimationsAtStart = 0;
};

struct EffectFile
{
    std::uint16_t version = EffectFileVersion5;
    std::string projectName;
    std::vector<EmitterResource> emitters;

    const EmitterResource* FindEmitter(std::string_view name) const;
};

EffectFile ReadEffectFile(ByteView bytes);

// One picture of a .breft file, in the graphics chip's own format.
struct EffectTexture
{
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    RgbaImage image;
};

std::vector<EffectTexture> ReadEffectTextureFile(ByteView bytes);

}
