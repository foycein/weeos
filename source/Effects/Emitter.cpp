#include "Effects/Emitter.h"

#include "Effects/AnimationCurve.h"
#include "Effects/EmitterForms.h"
#include "Effects/VectorMath.h"

#include <algorithm>
#include <cmath>

namespace Effects
{

using Formats::AnimationTarget;
using Formats::AnimationValueKind;
using Formats::EffectAnimation;
using Formats::Rgba8Color;
namespace EmitterFlag = Formats::EmitterFlag;
namespace EmitFlag = Formats::EmitFlag;

namespace
{

constexpr float PercentPerUnit = 100;
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

// The space of a field's numbers (FieldSettings::space) and what it moves (FieldSettings::mode).
constexpr std::uint8_t FieldInTheWorld = 0;
constexpr std::uint8_t FieldMovesThePosition = 1;

std::uint8_t ToByte(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
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

std::size_t ColorNumberOf(AnimationTarget target)
{
    std::size_t number = static_cast<std::size_t>(target) - static_cast<std::size_t>(AnimationTarget::Color1PrimaryRgb);
    return number / 2;
}

bool IsRgbTarget(AnimationTarget target)
{
    return target == AnimationTarget::Color1PrimaryRgb || target == AnimationTarget::Color1SecondaryRgb ||
        target == AnimationTarget::Color2PrimaryRgb || target == AnimationTarget::Color2SecondaryRgb;
}

bool IsAlphaTarget(AnimationTarget target)
{
    return target == AnimationTarget::Color1PrimaryAlpha || target == AnimationTarget::Color1SecondaryAlpha ||
        target == AnimationTarget::Color2PrimaryAlpha || target == AnimationTarget::Color2SecondaryAlpha;
}

void ApplyTextureTransform(Particle& particle, AnimationTarget target, const std::vector<float>& values)
{
    constexpr std::size_t TargetsPerTexture = 3;
    std::size_t number = static_cast<std::size_t>(target) - static_cast<std::size_t>(AnimationTarget::Texture1Scale);
    std::size_t texture = number / TargetsPerTexture;
    std::size_t part = number % TargetsPerTexture;
    Formats::TextureTransform& transform = particle.textureTransforms[texture];

    if (part == 0)
        SetFirstComponents(transform.scale, values);
    else if (part == 1 && !values.empty())
        transform.rotation = values[0];
    else
        SetFirstComponents(transform.translation, values);
}

void ApplyValues(Particle& particle, AnimationTarget target, const std::vector<float>& values)
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

    bool isTextureTransform = target >= AnimationTarget::Texture1Scale && target <= AnimationTarget::IndirectTextureTranslation;
    if (isTextureTransform)
    {
        ApplyTextureTransform(particle, target, values);
        return;
    }

    switch (target)
    {
    case AnimationTarget::FirstAlphaReference:
        if (!values.empty())
            particle.firstAlphaReference = ToByte(values[0]);
        break;
    case AnimationTarget::SecondAlphaReference:
        if (!values.empty())
            particle.secondAlphaReference = ToByte(values[0]);
        break;
    case AnimationTarget::Size:
        SetFirstComponents(particle.size, values);
        break;
    case AnimationTarget::Scale:
        SetFirstComponents(particle.scale, values);
        break;
    case AnimationTarget::Rotation:
        SetFirstComponents(particle.rotation, values);
        break;
    default:
        break;
    }
}

void ApplyPattern(Particle& particle, const EffectAnimation& animation, const CurveTime& time)
{
    const Formats::AnimationKey* key = FindKeyAtOrBefore(animation, time.frame);
    if (key == nullptr || key->textureNameIndex >= animation.names.size())
        return;

    std::size_t texture = static_cast<std::size_t>(animation.target) - static_cast<std::size_t>(AnimationTarget::Texture1Pattern);
    particle.textureNames[texture] = animation.names[key->textureNameIndex];

    std::size_t wrapShift = texture * BitsPerTextureWrap;
    particle.textureWraps = static_cast<std::uint16_t>(particle.textureWraps & ~(TextureWrapMask << wrapShift));
    particle.textureWraps = static_cast<std::uint16_t>(particle.textureWraps | ((key->textureWrap & TextureWrapMask) << wrapShift));

    std::size_t reverseShift = texture * BitsPerTextureReverse;
    particle.textureReverses = static_cast<std::uint8_t>(particle.textureReverses & ~(TextureReverseMask << reverseShift));
    particle.textureReverses = static_cast<std::uint8_t>(particle.textureReverses | ((key->textureReverse & TextureReverseMask) << reverseShift));
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
    Vector3 along = Multiply(axis, Dot(axis, point) * (1 - cosine));
    return Add(Add(Multiply(point, cosine), Multiply(Cross(axis, point), sine)), along);
}

struct FieldResult
{
    Vector3 positionPush = {0, 0, 0};
    Vector3 velocityChange = {0, 0, 0};
};

Vector3 VortexPush(const std::vector<float>& values, Vector3 relativePosition)
{
    Vector3 axis = UpTurnedBy({ValueAt(values, 3), ValueAt(values, 4), ValueAt(values, 5)});
    Vector3 across = Subtract(relativePosition, Multiply(axis, Dot(axis, relativePosition)));
    float distanceSquared = LengthSquared(across);
    if (distanceSquared <= 0)
        return {0, 0, 0};

    float innerSpeed = ValueAt(values, 0);
    float outerSpeed = ValueAt(values, 1);
    float radius = ValueAt(values, 2);
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
    Vector3 axis = UpTurnedBy({ValueAt(values, 1), ValueAt(values, 2), ValueAt(values, 3)});
    return Subtract(TurnAroundAxis(relativePosition, axis, ValueAt(values, 0)), relativePosition);
}

// Gravity, vortex and spin work out a vector in the world or in the emitter's space, which moves the position or changes the
// velocity; speed multiplies the velocity of the step's start.
void AddField(FieldResult& fields, const EffectAnimation& animation, const std::vector<float>& values, const Particle& particle,
    Vector3 startVelocity, const Graphics::Matrix4& world)
{
    if (animation.target == AnimationTarget::SpeedField)
    {
        fields.velocityChange = Add(fields.velocityChange, Multiply(startVelocity, ValueAt(values, 0) - 1));
        return;
    }

    bool isInEmitterSpace = animation.field.space != FieldInTheWorld;
    Vector3 relativePosition = Subtract(particle.position, ColumnOf(world, 3));
    if (isInEmitterSpace)
        relativePosition = Graphics::TransformPoint(InvertAffine(world), particle.position);

    Vector3 result = {0, 0, 0};
    if (animation.target == AnimationTarget::GravityField)
        result = Multiply({ValueAt(values, 1), ValueAt(values, 2), ValueAt(values, 3)}, ValueAt(values, 0));
    else if (animation.target == AnimationTarget::VortexField)
        result = VortexPush(values, relativePosition);
    else if (animation.target == AnimationTarget::SpinField)
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
    return static_cast<std::uint8_t>(value * percent / 100);
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
        if (onlyAtStart && age != 0)
            continue;
        if ((animation.flags & Formats::AnimationFlag::Disabled) != 0)
            continue;

        CurveTime time = FindCurveTime(animation, age, settings.emitterLife, resource.version);
        std::vector<float> values = EvaluateCurve(animation, time, 0, resource.version);
        switch (animation.target)
        {
        case AnimationTarget::EmitterFormParameters:
            for (std::size_t number = 0; number < values.size() && number < settings.formParameters.size(); ++number)
                settings.formParameters[number] = values[number];
            break;
        case AnimationTarget::EmitterScale: SetFirstComponents(settings.scale, values); break;
        case AnimationTarget::EmitterRotation: SetFirstComponents(settings.rotation, values); break;
        case AnimationTarget::EmitterTranslation: SetFirstComponents(settings.translation, values); break;
        case AnimationTarget::EmitterRadiationSpeed: settings.radiationSpeed = ValueAt(values, 0); break;
        case AnimationTarget::EmitterYAxisSpeed: settings.yAxisSpeed = ValueAt(values, 0); break;
        case AnimationTarget::EmitterRandomDirectionSpeed: settings.randomDirectionSpeed = ValueAt(values, 0); break;
        case AnimationTarget::EmitterNormalSpeed:
            settings.normalSpeed = ValueAt(values, 0);
            settings.normalDiffusion = ValueAt(values, 1);
            break;
        case AnimationTarget::EmitterSpecialDirectionSpeed:
            settings.specialDirectionSpeed = ValueAt(values, 0);
            settings.specialDirectionDiffusion = ValueAt(values, 1);
            break;
        case AnimationTarget::EmitterEmitCount: settings.emitCount = ValueAt(values, 0); break;
        default: break;
        }
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
        bool onlyAtBirth = index < resource.particleAnimationsAtBirth;
        if (onlyAtBirth && particle.age != 0)
            continue;
        if ((animation.flags & Formats::AnimationFlag::Disabled) != 0)
            continue;

        bool isField = animation.kind == AnimationValueKind::Field;
        bool isChild = animation.kind == AnimationValueKind::Child;
        bool isStartValue = animation.frameCount <= 1 && !isField && !isChild;
        if (isStartValue && particle.age != 0)
            continue;

        bool usesEmitterTime = (animation.flags & Formats::AnimationFlag::UsesEmitterTime) != 0;
        std::uint32_t curveAge = usesEmitterTime ? age : particle.age;
        std::uint32_t curveLife = usesEmitterTime ? settings.emitterLife : particle.life;
        CurveTime time = FindCurveTime(animation, curveAge, curveLife, resource.version);

        if (isChild)
            FireChildren(animation, particle, curveAge, curveLife, childQueue);
        else if (animation.kind == AnimationValueKind::TexturePattern)
            ApplyPattern(particle, animation, time);
        else if (isField)
            AddField(fields, animation, FieldValues(animation, time, particle.seed, resource.version), particle, startVelocity, world);
        else
            ApplyValues(particle, animation.target, EvaluateCurve(animation, time, particle.seed, resource.version));
    }

    ++particle.age;
    particle.velocity = Add(particle.velocity, fields.velocityChange);
    particle.position = Add(particle.position, Multiply(Add(fields.positionPush, particle.velocity), particle.momentum));
    return true;
}

// A child key fires in the step in which the curve comes to its frame.
void Emitter::FireChildren(const EffectAnimation& animation, const Particle& particle, std::uint32_t curveAge, std::uint32_t curveLife,
    std::vector<ChildBirth>& childQueue) const
{
    CurveTime time = FindCurveTime(animation, curveAge, curveLife, resource.version);
    CurveTime timeBefore = FindCurveTime(animation, curveAge == 0 ? 0 : curveAge - 1, curveLife, resource.version);
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

    float amount = EmitAmount();
    auto count = static_cast<std::uint32_t>(amount);
    MakeParticles(count);
}

float Emitter::EmitAmount()
{
    if ((settings.emitFlags & EmitFlag::FixedCount) != 0)
        return settings.fixedCount;

    float amount = settings.emitCount;
    if (settings.emitCountRandomPercent != 0)
        amount += amount * settings.emitCountRandomPercent / PercentPerUnit * (2 * random.NextFraction() - 1);

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

    std::uint32_t life = settings.particleLife;
    if (settings.particleLifeRandomPercent != 0)
        life -= static_cast<std::uint32_t>(settings.particleLifeRandomPercent / PercentPerUnit * life * random.NextFraction());
    particle.life = std::clamp<std::uint32_t>(life, 1, LongestLife);

    particle.momentum = 1 + settings.momentumRandomPercent / PercentPerUnit * random.NextFraction();
    particle.seed = static_cast<std::uint16_t>(random.NextBits());

    if (start.sizeRandomPercent != 0)
    {
        float change = static_cast<float>(random.NextSigned()) * start.sizeRandomPercent / SizeRandomDivisor;
        particle.size.x += particle.size.x * change;
        particle.size.y += particle.size.y * change;
    }

    std::array<float, 3> offset = {start.rotationOffset.x, start.rotationOffset.y, start.rotationOffset.z};
    for (std::size_t axis = 0; axis < offset.size(); ++axis)
    {
        float randomShare = start.rotationOffsetRandomPercent[axis] / PercentPerUnit * (2 * random.NextFraction() - 1);
        offset[axis] += offset[axis] * randomShare;
    }
    particle.rotationOffset = {offset[0], offset[1], offset[2]};

    InheritFromTheSource(particle);
    return particle;
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
