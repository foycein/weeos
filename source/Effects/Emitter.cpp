#include "Effects/Emitter.h"

#include "Effects/AnimationCurve.h"
#include "Effects/EmitterForms.h"
#include "Effects/VectorMath.h"

#include <algorithm>
#include <cmath>

namespace Effects
{

using Formats::EffectAnimationTarget;
using Formats::AnimationValueKind;
using Formats::EffectAnimation;
using Formats::Rgba8Color;
namespace EmitterFlag = Formats::EmitterFlag;
namespace EmitFlag = Formats::EmitFlag;

namespace
{

constexpr float PercentPerUnit = 100;
constexpr int WholePercent = 100;
constexpr float HighestByte = 255;
constexpr std::uint32_t LongestLife = 0xFFFF;

// Version 5's size random: size * (a signed 16-bit random number) * percent / (32768 * 100).
constexpr float SizeRandomDivisor = 3276800;

constexpr std::size_t RedComponent = 0;
constexpr std::size_t GreenComponent = 1;
constexpr std::size_t BlueComponent = 2;
constexpr std::size_t BitsPerTextureWrap = 4;
constexpr std::uint16_t TextureWrapMask = 0xF;
constexpr std::size_t BitsPerTextureReverse = 2;
constexpr std::uint8_t TextureReverseMask = 0x3;

// The color targets come in pairs: the colors of a register, then its alpha.
constexpr std::size_t TargetsPerColor = 2;

// The space of a field's numbers (FieldSettings::space) and what it moves (FieldSettings::mode).
constexpr std::uint8_t FieldInTheWorld = 0;
constexpr std::uint8_t FieldMovesThePosition = 1;

// Where the numbers of each field begin in its values.
constexpr std::size_t FieldStrength = 0;
constexpr std::size_t GravityDirection = 1;
constexpr std::size_t VortexInnerSpeed = 0;
constexpr std::size_t VortexOuterSpeed = 1;
constexpr std::size_t VortexRadius = 2;
constexpr std::size_t VortexAxisAngles = 3;
constexpr std::size_t SpinAngle = 0;
constexpr std::size_t SpinAxisAngles = 1;

std::uint8_t ToByte(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, HighestByte));
}

void SetRgb(Rgba8Color& color, const std::vector<float>& values)
{
    if (values.size() <= BlueComponent)
        return;
    color.red = ToByte(values[RedComponent]);
    color.green = ToByte(values[GreenComponent]);
    color.blue = ToByte(values[BlueComponent]);
}

void SetFirstComponents(Vector2& vector, const std::vector<float>& values)
{
    if (!values.empty())
        vector.x = values[0];
    if (values.size() > 1)
        vector.y = values[1];
}

void SetFirstComponents(Vector3& vector, const std::vector<float>& values)
{
    if (!values.empty())
        vector.x = values[0];
    if (values.size() > 1)
        vector.y = values[1];
    if (values.size() > 2)
        vector.z = values[2];
}

std::size_t ColorNumberOf(EffectAnimationTarget target)
{
    std::size_t number = static_cast<std::size_t>(target) - static_cast<std::size_t>(EffectAnimationTarget::Color1PrimaryRgb);
    return number / TargetsPerColor;
}

bool IsRgbTarget(EffectAnimationTarget target)
{
    return target == EffectAnimationTarget::Color1PrimaryRgb || target == EffectAnimationTarget::Color1SecondaryRgb ||
        target == EffectAnimationTarget::Color2PrimaryRgb || target == EffectAnimationTarget::Color2SecondaryRgb;
}

bool IsAlphaTarget(EffectAnimationTarget target)
{
    return target == EffectAnimationTarget::Color1PrimaryAlpha || target == EffectAnimationTarget::Color1SecondaryAlpha ||
        target == EffectAnimationTarget::Color2PrimaryAlpha || target == EffectAnimationTarget::Color2SecondaryAlpha;
}

void ApplyTextureTransform(Particle& particle, EffectAnimationTarget target, const std::vector<float>& values)
{
    constexpr std::size_t TargetsPerTexture = 3;
    constexpr std::size_t ScalePart = 0;
    constexpr std::size_t RotationPart = 1;
    std::size_t number = static_cast<std::size_t>(target) - static_cast<std::size_t>(EffectAnimationTarget::Texture1Scale);
    std::size_t texture = number / TargetsPerTexture;
    std::size_t part = number % TargetsPerTexture;
    Formats::ParticleTextureTransform& transform = particle.textureTransforms[texture];

    if (part == ScalePart)
        SetFirstComponents(transform.scale, values);
    else if (part == RotationPart && !values.empty())
        transform.rotation = values[0];
    else
        SetFirstComponents(transform.translation, values);
}

void ApplyValues(Particle& particle, EffectAnimationTarget target, const std::vector<float>& values)
{
    if (IsRgbTarget(target))
    {
        SetRgb(particle.colors[ColorNumberOf(target)], values);
        return;
    }
    if (IsAlphaTarget(target))
    {
        if (!values.empty())
            particle.colors[ColorNumberOf(target)].alpha = ToByte(values[0]);
        return;
    }

    bool isTextureTransform = target >= EffectAnimationTarget::Texture1Scale && target <= EffectAnimationTarget::IndirectTextureTranslation;
    if (isTextureTransform)
    {
        ApplyTextureTransform(particle, target, values);
        return;
    }

    switch (target)
    {
    case EffectAnimationTarget::FirstAlphaReference:
        if (!values.empty())
            particle.firstAlphaReference = ToByte(values[0]);
        break;
    case EffectAnimationTarget::SecondAlphaReference:
        if (!values.empty())
            particle.secondAlphaReference = ToByte(values[0]);
        break;
    case EffectAnimationTarget::Size:
        SetFirstComponents(particle.size, values);
        break;
    case EffectAnimationTarget::Scale:
        SetFirstComponents(particle.scale, values);
        break;
    case EffectAnimationTarget::Rotation:
        SetFirstComponents(particle.rotation, values);
        break;
    default:
        break;
    }
}

// The bits under mask << shift replaced by the value's lowest bits.
std::uint32_t WithBitsReplaced(std::uint32_t bits, std::uint32_t mask, std::size_t shift, std::uint32_t value)
{
    std::uint32_t cleared = bits & ~(mask << shift);
    return cleared | ((value & mask) << shift);
}

void ApplyPattern(Particle& particle, const EffectAnimation& animation, const CurveTime& time)
{
    const Formats::AnimationKey* key = FindKeyAtOrBefore(animation, time.frame);
    if (key == nullptr || key->textureNameIndex >= animation.names.size())
        return;

    std::size_t texture = static_cast<std::size_t>(animation.target) - static_cast<std::size_t>(EffectAnimationTarget::Texture1Pattern);
    particle.textureNames[texture] = animation.names[key->textureNameIndex];

    std::size_t wrapShift = texture * BitsPerTextureWrap;
    std::uint32_t wraps = WithBitsReplaced(particle.textureWraps, TextureWrapMask, wrapShift, key->textureWrap);
    particle.textureWraps = static_cast<std::uint16_t>(wraps);

    std::size_t reverseShift = texture * BitsPerTextureReverse;
    std::uint32_t reverses = WithBitsReplaced(particle.textureReverses, TextureReverseMask, reverseShift, key->textureReverse);
    particle.textureReverses = static_cast<std::uint8_t>(reverses);
}

// The numbers of a field: those of its settings, of which the curve's keys replace the first ones.
std::vector<float> FieldValues(const EffectAnimation& animation, const CurveTime& time, std::uint16_t particleSeed, std::uint16_t version)
{
    std::vector<float> values = animation.field.values;
    std::vector<float> curve = EvaluateCurve(animation, time, particleSeed, version);
    for (std::size_t number = 0; number < curve.size() && number < values.size(); ++number)
        values[number] = curve[number];
    return values;
}

float ValueAt(const std::vector<float>& values, std::size_t index)
{
    return index < values.size() ? values[index] : 0.0f;
}

Vector3 TurnAroundAxis(Vector3 point, Vector3 axis, float angle)
{
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    Vector3 kept = Multiply(point, cosine);
    Vector3 around = Multiply(Cross(axis, point), sine);
    Vector3 along = Multiply(axis, Dot(axis, point) * (1 - cosine));
    return Add(Add(kept, around), along);
}

struct FieldResult
{
    Vector3 positionPush = {0, 0, 0};
    Vector3 velocityChange = {0, 0, 0};
};

Vector3 VectorAt(const std::vector<float>& values, std::size_t index)
{
    return {ValueAt(values, index), ValueAt(values, index + 1), ValueAt(values, index + 2)};
}

Vector3 VortexPush(const std::vector<float>& values, Vector3 relativePosition)
{
    Vector3 axis = UpTurnedBy(VectorAt(values, VortexAxisAngles));
    Vector3 across = Subtract(relativePosition, Multiply(axis, Dot(axis, relativePosition)));
    float distanceSquared = LengthSquared(across);
    if (distanceSquared <= 0)
        return {0, 0, 0};

    float innerSpeed = ValueAt(values, VortexInnerSpeed);
    float outerSpeed = ValueAt(values, VortexOuterSpeed);
    float radius = ValueAt(values, VortexRadius);
    float speed = outerSpeed;
    if (distanceSquared < radius * radius)
    {
        float share = distanceSquared / (radius * radius);
        speed = (1 - share) * innerSpeed + share * outerSpeed;
    }
    return Multiply(Cross(NormalizeOr(across, across), axis), speed);
}

Vector3 SpinPush(const std::vector<float>& values, Vector3 relativePosition)
{
    Vector3 axis = UpTurnedBy(VectorAt(values, SpinAxisAngles));
    Vector3 turned = TurnAroundAxis(relativePosition, axis, ValueAt(values, SpinAngle));
    return Subtract(turned, relativePosition);
}

// Gravity, vortex and spin work out a vector in the world or in the emitter's space, which moves the position or changes the
// velocity; speed multiplies the velocity of the step's start.
void AddField(FieldResult& fields, const EffectAnimation& animation, const std::vector<float>& values, const Particle& particle,
    Vector3 startVelocity, const Graphics::Matrix4& world)
{
    if (animation.target == EffectAnimationTarget::SpeedField)
    {
        float speedFactor = ValueAt(values, FieldStrength);
        fields.velocityChange = Add(fields.velocityChange, Multiply(startVelocity, speedFactor - 1));
        return;
    }

    bool isInEmitterSpace = animation.field.space != FieldInTheWorld;
    Vector3 relativePosition = Subtract(particle.position, ColumnOf(world, TranslationColumn));
    if (isInEmitterSpace)
        relativePosition = Graphics::TransformPoint(InvertAffine(world), particle.position);

    Vector3 result = {0, 0, 0};
    if (animation.target == EffectAnimationTarget::GravityField)
        result = Multiply(VectorAt(values, GravityDirection), ValueAt(values, FieldStrength));
    else if (animation.target == EffectAnimationTarget::VortexField)
        result = VortexPush(values, relativePosition);
    else if (animation.target == EffectAnimationTarget::SpinField)
        result = SpinPush(values, relativePosition);

    if (isInEmitterSpace)
        result = Graphics::TransformDirection(WithoutTranslation(world), result);

    if (animation.field.mode == FieldMovesThePosition)
        fields.positionPush = Add(fields.positionPush, result);
    else
        fields.velocityChange = Add(fields.velocityChange, result);
}

Graphics::Matrix4 InheritedParts(const Graphics::Matrix4& world, bool takesScale, bool takesRotation)
{
    Graphics::Matrix4 turnAndScale = WithoutTranslation(world);
    if (takesScale && takesRotation)
        return turnAndScale;

    Graphics::Matrix4 result;
    for (int column = 0; column < 3; ++column)
    {
        Vector3 axis = ColumnOf(turnAndScale, column);
        float length = Length(axis);
        Vector3 unit = {0, 0, 0};
        unit.x = column == 0 ? 1.0f : 0.0f;
        unit.y = column == 1 ? 1.0f : 0.0f;
        unit.z = column == 2 ? 1.0f : 0.0f;
        if (takesScale)
            unit = Multiply(unit, length);
        if (takesRotation)
            unit = NormalizeOr(axis, unit);
        result.rows[0][column] = unit.x;
        result.rows[1][column] = unit.y;
        result.rows[2][column] = unit.z;
    }
    return result;
}

std::uint8_t PercentOf(std::uint8_t value, std::uint8_t percent)
{
    return static_cast<std::uint8_t>(value * percent / WholePercent);
}

void ApplyEmitterValues(Formats::EmitterSettings& settings, EffectAnimationTarget target, const std::vector<float>& values)
{
    float first = ValueAt(values, 0);
    float second = ValueAt(values, 1);
    switch (target)
    {
    case EffectAnimationTarget::EmitterFormParameters:
        for (std::size_t number = 0; number < values.size() && number < settings.formParameters.size(); ++number)
            settings.formParameters[number] = values[number];
        break;
    case EffectAnimationTarget::EmitterScale:
        SetFirstComponents(settings.scale, values);
        break;
    case EffectAnimationTarget::EmitterRotation:
        SetFirstComponents(settings.rotation, values);
        break;
    case EffectAnimationTarget::EmitterTranslation:
        SetFirstComponents(settings.translation, values);
        break;
    case EffectAnimationTarget::EmitterRadiationSpeed:
        settings.radiationSpeed = first;
        break;
    case EffectAnimationTarget::EmitterYAxisSpeed:
        settings.yAxisSpeed = first;
        break;
    case EffectAnimationTarget::EmitterRandomDirectionSpeed:
        settings.randomDirectionSpeed = first;
        break;
    case EffectAnimationTarget::EmitterNormalSpeed:
        settings.normalSpeed = first;
        settings.normalDiffusion = second;
        break;
    case EffectAnimationTarget::EmitterSpecialDirectionSpeed:
        settings.specialDirectionSpeed = first;
        settings.specialDirectionDiffusion = second;
        break;
    case EffectAnimationTarget::EmitterEmitCount:
        settings.emitCount = first;
        break;
    default:
        break;
    }
}

// An animation that runs only at the start runs while the age is 0; a disabled one never runs.
bool RunsAtThisAge(const EffectAnimation& animation, bool onlyAtTheStart, std::uint32_t animatedAge)
{
    if (onlyAtTheStart && animatedAge != 0)
        return false;
    return (animation.flags & Formats::AnimationFlag::Disabled) == 0;
}

}

Emitter::Emitter(const Formats::EmitterResource& emitterResource, std::uint16_t randomSeed, std::uint8_t priority)
    : resource(emitterResource)
    , settings(emitterResource.emitter)
    , random(randomSeed)
    , drawPriority(priority)
    , startDelay(emitterResource.emitter.emitStartDelay)
    , preRollStepsLeft(emitterResource.emitter.preRollSteps)
{
}

const Formats::EmitterResource& Emitter::Resource() const
{
    return resource;
}

Formats::EmitterSettings& Emitter::Settings()
{
    return settings;
}

const Formats::EmitterSettings& Emitter::Settings() const
{
    return settings;
}

void Emitter::SetParentMatrix(const Graphics::Matrix4& matrix)
{
    parentMatrix = matrix;
}

void Emitter::SetInheritance(const ChildInheritance& childInheritance)
{
    inheritance = childInheritance;
}

Graphics::Matrix4 Emitter::WorldMatrix() const
{
    return parentMatrix * Graphics::MakeTranslation(settings.translation) * MakeRotationXyz(settings.rotation) *
        Graphics::MakeScale(settings.scale);
}

Graphics::Matrix4 Emitter::ParticleSpaceMatrix() const
{
    bool takesScale = (settings.commonFlags & EmitterFlag::ParticlesInheritScale) != 0;
    bool takesRotation = (settings.commonFlags & EmitterFlag::ParticlesInheritRotation) != 0;
    return InheritedParts(WorldMatrix(), takesScale, takesRotation);
}

void Emitter::Step(std::vector<ChildBirth>& childQueue)
{
    if ((settings.commonFlags & EmitterFlag::Paused) != 0)
        return;

    ApplyEmitterAnimations();
    CheckLife();
    UpdateParticles(0, childQueue);

    std::size_t firstNewParticle = particles.size();
    Emit();
    ++age;
    UpdateParticles(firstNewParticle, childQueue);
}

void Emitter::Stop()
{
    isEmitting = false;
}

bool Emitter::IsEmitting() const
{
    return isEmitting;
}

bool Emitter::HasParticles() const
{
    return !particles.empty();
}

bool Emitter::NeedsPreRoll() const
{
    return preRollStepsLeft > 0;
}

void Emitter::CountPreRollStep()
{
    if (preRollStepsLeft > 0)
        --preRollStepsLeft;
}

const std::vector<Particle>& Emitter::Particles() const
{
    return particles;
}

std::uint8_t Emitter::DrawPriority() const
{
    return drawPriority;
}

void Emitter::ApplyEmitterAnimations()
{
    for (std::size_t index = 0; index < resource.emitterAnimations.size(); ++index)
    {
        const EffectAnimation& animation = resource.emitterAnimations[index];
        bool onlyAtStart = index < resource.emitterAnimationsAtStart;
        if (!RunsAtThisAge(animation, onlyAtStart, age))
            continue;

        CurveTime time = FindCurveTime(animation, age, settings.emitterLife, resource.version);
        ApplyEmitterValues(settings, animation.target, EvaluateCurve(animation, time, 0, resource.version));
    }
}

void Emitter::CheckLife()
{
    bool isEndless = (settings.commonFlags & EmitterFlag::Endless) != 0;
    if (!isEndless && age >= settings.emitterLife)
        isEmitting = false;
}

void Emitter::UpdateParticles(std::size_t firstParticle, std::vector<ChildBirth>& childQueue)
{
    std::vector<Particle> survivors(particles.begin(), particles.begin() + static_cast<std::ptrdiff_t>(firstParticle));
    for (std::size_t index = firstParticle; index < particles.size(); ++index)
    {
        Particle particle = particles[index];
        if (UpdateParticle(particle, childQueue))
            survivors.push_back(particle);
    }
    particles = std::move(survivors);
}

bool Emitter::UpdateParticle(Particle& particle, std::vector<ChildBirth>& childQueue)
{
    if (particle.life <= particle.age)
        return false;

    particle.previousPosition = particle.position;
    FieldResult fields;
    Graphics::Matrix4 world = WorldMatrix();
    Vector3 startVelocity = particle.velocity;

    for (std::size_t index = 0; index < resource.particleAnimations.size(); ++index)
    {
        const EffectAnimation& animation = resource.particleAnimations[index];
        bool isField = animation.kind == AnimationValueKind::Field;
        bool isChild = animation.kind == AnimationValueKind::Child;
        bool isStartValue = animation.frameCount <= 1 && !isField && !isChild;
        bool onlyAtBirth = index < resource.particleAnimationsAtBirth || isStartValue;
        if (!RunsAtThisAge(animation, onlyAtBirth, particle.age))
            continue;

        bool usesEmitterTime = (animation.flags & Formats::AnimationFlag::UsesEmitterTime) != 0;
        std::uint32_t curveAge = usesEmitterTime ? age : particle.age;
        std::uint32_t curveLife = usesEmitterTime ? settings.emitterLife : particle.life;
        CurveTime time = FindCurveTime(animation, curveAge, curveLife, resource.version);

        if (isChild)
        {
            FireChildren(animation, particle, curveAge, curveLife, childQueue);
            continue;
        }
        if (animation.kind == AnimationValueKind::TexturePattern)
        {
            ApplyPattern(particle, animation, time);
            continue;
        }
        if (isField)
        {
            std::vector<float> fieldValues = FieldValues(animation, time, particle.seed, resource.version);
            AddField(fields, animation, fieldValues, particle, startVelocity, world);
            continue;
        }
        ApplyValues(particle, animation.target, EvaluateCurve(animation, time, particle.seed, resource.version));
    }

    ++particle.age;
    particle.velocity = Add(particle.velocity, fields.velocityChange);
    Vector3 move = Add(fields.positionPush, particle.velocity);
    particle.position = Add(particle.position, Multiply(move, particle.momentum));
    return true;
}

// A child key fires in the step in which the curve comes to its frame.
void Emitter::FireChildren(const EffectAnimation& animation, const Particle& particle, std::uint32_t curveAge, std::uint32_t curveLife,
    std::vector<ChildBirth>& childQueue) const
{
    CurveTime time = FindCurveTime(animation, curveAge, curveLife, resource.version);
    std::uint32_t ageBefore = curveAge == 0 ? 0 : curveAge - 1;
    CurveTime timeBefore = FindCurveTime(animation, ageBefore, curveLife, resource.version);
    bool hasEnteredTheFrame = curveAge == 0 || timeBefore.frame != time.frame;
    if (!hasEnteredTheFrame)
        return;

    bool takesScale = (settings.commonFlags & EmitterFlag::ChildrenInheritScale) != 0;
    bool takesRotation = (settings.commonFlags & EmitterFlag::ChildrenInheritRotation) != 0;
    Graphics::Matrix4 parent = Graphics::MakeTranslation(particle.position) * InheritedParts(WorldMatrix(), takesScale, takesRotation);

    for (const Formats::AnimationKey& key : animation.keys)
    {
        bool fires = key.frame == time.frame && key.child.nameIndex < animation.names.size();
        if (fires)
            childQueue.push_back({animation.names[key.child.nameIndex], key.child, particle, parent, resource.shape.heightRule});
    }
}

void Emitter::Emit()
{
    if (!isEmitting)
        return;

    if (startDelay > 0)
    {
        --startDelay;
        return;
    }

    if (intervalCounter > 0)
    {
        --intervalCounter;
        return;
    }
    intervalCounter = settings.emitInterval;

    auto count = static_cast<std::uint32_t>(EmitAmount());
    MakeParticles(count);
}

float Emitter::EmitAmount()
{
    if ((settings.emitFlags & EmitFlag::FixedCount) != 0)
        return settings.fixedCount;

    float amount = settings.emitCount;
    if (settings.emitCountRandomPercent != 0)
    {
        float signedFraction = 2 * random.NextFraction() - 1;
        amount += amount * settings.emitCountRandomPercent / PercentPerUnit * signedFraction;
    }

    emitAccumulator += amount;
    if (isFirstEmission && settings.emitCount != 0 && emitAccumulator < 1)
        emitAccumulator = 1;
    isFirstEmission = false;

    float whole = std::floor(emitAccumulator);
    emitAccumulator -= whole;
    return whole;
}

void Emitter::MakeParticles(std::uint32_t count)
{
    if (count == 0)
        return;

    for (const FormPlacement& placement : PlaceParticles(settings, count, random))
        particles.push_back(NewParticle(placement.position, InitialVelocity(settings, placement, random)));
}

Particle Emitter::NewParticle(Vector3 localPosition, Vector3 localVelocity)
{
    const Formats::ParticleParameters& start = resource.particle;
    Graphics::Matrix4 world = WorldMatrix();

    Particle particle;
    particle.position = Graphics::TransformPoint(world, localPosition);
    particle.previousPosition = particle.position;
    particle.velocity = Graphics::TransformDirection(world, localVelocity);
    particle.colors = start.colors;
    particle.size = start.size;
    particle.scale = start.scale;
    particle.rotation = start.rotation;
    particle.textureTransforms = start.textureTransforms;
    for (std::size_t texture = 0; texture < start.textureNames.size(); ++texture)
        particle.textureNames[texture] = start.textureNames[texture];
    particle.textureWraps = start.textureWraps;
    particle.textureReverses = start.textureReverses;
    particle.firstAlphaReference = start.firstAlphaReference;
    particle.secondAlphaReference = start.secondAlphaReference;

    particle.life = RandomParticleLife();
    particle.momentum = 1 + settings.momentumRandomPercent / PercentPerUnit * random.NextFraction();
    particle.seed = static_cast<std::uint16_t>(random.NextBits());
    RandomizeSize(particle);
    particle.rotationOffset = RandomRotationOffset();

    InheritFromTheSource(particle);
    return particle;
}

std::uint32_t Emitter::RandomParticleLife()
{
    std::uint32_t life = settings.particleLife;
    if (settings.particleLifeRandomPercent != 0)
    {
        float shortening = settings.particleLifeRandomPercent / PercentPerUnit * life * random.NextFraction();
        life -= static_cast<std::uint32_t>(shortening);
    }
    return std::clamp<std::uint32_t>(life, 1, LongestLife);
}

void Emitter::RandomizeSize(Particle& particle)
{
    std::int8_t randomPercent = resource.particle.sizeRandomPercent;
    if (randomPercent == 0)
        return;

    float change = static_cast<float>(random.NextSigned()) * randomPercent / SizeRandomDivisor;
    particle.size.x += particle.size.x * change;
    particle.size.y += particle.size.y * change;
}

Vector3 Emitter::RandomRotationOffset()
{
    const Formats::ParticleParameters& start = resource.particle;
    std::array<float, 3> offset = {start.rotationOffset.x, start.rotationOffset.y, start.rotationOffset.z};
    for (std::size_t axis = 0; axis < offset.size(); ++axis)
    {
        float signedFraction = 2 * random.NextFraction() - 1;
        float randomShare = start.rotationOffsetRandomPercent[axis] / PercentPerUnit * signedFraction;
        offset[axis] += offset[axis] * randomShare;
    }
    return {offset[0], offset[1], offset[2]};
}

void Emitter::InheritFromTheSource(Particle& particle) const
{
    if (!inheritance)
        return;

    const Formats::ChildRequest& request = inheritance->request;
    const Particle& source = inheritance->source;
    if (request.velocityPercent != 0)
        particle.velocity = Add(particle.velocity, Multiply(source.velocity, request.velocityPercent / PercentPerUnit));

    if (request.sizePercent != 0)
    {
        particle.size.x = request.sizePercent / PercentPerUnit * source.size.x * source.scale.x;
        particle.size.y = request.sizePercent / PercentPerUnit * ParticleHeight(source, inheritance->sourceHeightRule);
    }

    for (std::size_t number = 0; number < particle.colors.size(); ++number)
    {
        Rgba8Color& color = particle.colors[number];
        const Rgba8Color& sourceColor = source.colors[number];
        if (request.alphaPercent != 0)
            color.alpha = PercentOf(sourceColor.alpha, request.alphaPercent);
        if (request.colorPercent != 0)
        {
            color.red = PercentOf(sourceColor.red, request.colorPercent);
            color.green = PercentOf(sourceColor.green, request.colorPercent);
            color.blue = PercentOf(sourceColor.blue, request.colorPercent);
        }
    }
}

}
