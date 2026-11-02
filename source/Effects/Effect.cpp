#include "Effects/Effect.h"

#include "Effects/EffectError.h"
#include "Effects/EmitterForms.h"
#include "Effects/ParticleDrawing.h"

#include <algorithm>
#include <format>
#include <set>
#include <string>

namespace Effects
{

using Formats::EffectAnimationTarget;
using Formats::AnimationValueKind;
using Formats::EmitterResource;
using Formats::ParticleDrawType;

namespace
{

// Emit flags of features that only effects of WeeOS's own server use: the distance from the camera changes the amount, the emitter
// turns to the camera.
constexpr std::uint32_t EmitterTurnsToTheCamera = 0x8000;
constexpr std::uint32_t EmitterTurnsToTheCameraOnY = 0x10000;
constexpr std::uint8_t AmbientLighting = 1;

[[noreturn]] void FailNotBuilt(const EmitterResource& resource, std::string_view what)
{
    throw EffectError(std::format("the effect {} needs {}, which WeeOS does not build", resource.name, what));
}

bool IsShapeBuilt(const Formats::ParticleShape& shape)
{
    switch (shape.type)
    {
    case ParticleDrawType::Billboard:
        return shape.option == Formats::BillboardOption::FacesTheViewer || shape.option == Formats::BillboardOption::AlongTheDirection;
    case ParticleDrawType::Directional:
        return shape.direction == Formats::ParticleDirection::Velocity || shape.direction == Formats::ParticleDirection::FromTheEmitter;
    case ParticleDrawType::SmoothStripe:
        return (shape.stripeFlags & Formats::StripeConnection::Mask) == Formats::StripeConnection::BackToTheFirst;
    default:
        return false;
    }
}

bool IsAnimationBuilt(const Formats::EffectAnimation& animation)
{
    switch (animation.target)
    {
    case EffectAnimationTarget::MagnetField:
    case EffectAnimationTarget::NewtonField:
    case EffectAnimationTarget::TailField:
    case EffectAnimationTarget::EmitterUnnamed:
        return false;
    default:
        break;
    }

    for (const Formats::AnimationKey& key : animation.keys)
    {
        bool isRandomPattern = animation.kind == AnimationValueKind::TexturePattern && key.randomFlags != 0;
        bool makesParticles = animation.kind == AnimationValueKind::Child && !key.child.makesEmitter;
        if (isRandomPattern || makesParticles)
            return false;
    }
    return true;
}

}

void CheckEmitterIsBuilt(const EmitterResource& resource)
{
    const Formats::EmitterSettings& settings = resource.emitter;
    if (!IsFormBuilt(settings))
        FailNotBuilt(resource, std::format("the emitter form {}", static_cast<int>(settings.form)));

    std::uint32_t cameraFlags = Formats::EmitFlag::LevelOfDetail | EmitterTurnsToTheCamera | EmitterTurnsToTheCameraOnY;
    bool hasCameraFeature = (settings.emitFlags & cameraFlags) != 0;
    if (hasCameraFeature)
        FailNotBuilt(resource, "an emitter that follows the camera");
    if (settings.emitIntervalRandomPercent != 0)
        FailNotBuilt(resource, "a random emit interval");

    const Formats::ParticleShape& shape = resource.shape;
    if (!IsShapeBuilt(shape))
        FailNotBuilt(resource, std::format("the particle draw type {} option {}", static_cast<int>(shape.type), shape.option));

    const Formats::EffectDrawSetting& drawSetting = resource.drawSetting;
    if (!BlendModeOf(drawSetting))
        FailNotBuilt(resource, "a blend of the graphics chip other than over, replace and add");
    if (drawSetting.flickerType != 0)
        FailNotBuilt(resource, "a flickering alpha");
    if (drawSetting.lightingType > AmbientLighting)
        FailNotBuilt(resource, "a point light");

    for (const Formats::EffectAnimation& animation : resource.particleAnimations)
    {
        if (!IsAnimationBuilt(animation))
            FailNotBuilt(resource, std::format("the particle animation {}", static_cast<int>(animation.target)));
    }
    for (const Formats::EffectAnimation& animation : resource.emitterAnimations)
    {
        if (!IsAnimationBuilt(animation))
            FailNotBuilt(resource, "an emitter animation of version 11");
    }
}

namespace
{

// The emitter and every child it can make, each once.
void CheckEmitterAndChildren(const EffectResources& resources, const EmitterResource& resource, std::set<std::string>& checked)
{
    if (!checked.insert(resource.name).second)
        return;

    CheckEmitterIsBuilt(resource);
    for (const Formats::EffectAnimation& animation : resource.particleAnimations)
    {
        if (animation.kind != AnimationValueKind::Child)
            continue;

        for (const std::string& childName : animation.names)
        {
            const EmitterResource* child = resources.FindEmitter(childName);
            if (child == nullptr)
                throw EffectError(std::format("the effect {} makes {}, which no loaded file has", resource.name, childName));
            CheckEmitterAndChildren(resources, *child, checked);
        }
    }
}

}

Effect::Effect(EffectResources& effectResources, std::string_view emitterName, std::uint16_t randomSeed)
    : resources(effectResources)
    , random(randomSeed)
{
    const EmitterResource* resource = resources.FindEmitter(emitterName);
    if (resource == nullptr)
        throw EffectError(std::format("no loaded effect file has the effect {}", emitterName));

    std::set<std::string> checked;
    CheckEmitterAndChildren(resources, *resource, checked);
    emitters.push_back(MakeEmitter(*resource, FirstEmitterPriority));
}

std::unique_ptr<Emitter> Effect::MakeEmitter(const EmitterResource& resource, std::uint8_t priority)
{
    std::uint16_t seed = resource.emitter.randomSeed;
    if (seed == 0)
        seed = static_cast<std::uint16_t>(random.NextBits());

    auto emitter = std::make_unique<Emitter>(resource, seed, priority);
    emitter->SetParentMatrix(matrix);
    return emitter;
}

void Effect::SetMatrix(const Graphics::Matrix4& newMatrix)
{
    matrix = newMatrix;
    emitters.front()->SetParentMatrix(matrix);
}

void Effect::SetTranslation(Formats::Vector3 translation)
{
    SetMatrix(Graphics::MakeTranslation(translation));
}

void Effect::SetRotationOffset(Formats::Vector3 radians)
{
    rotationOffset = radians;
}

Formats::Vector3 Effect::RotationOffset() const
{
    return rotationOffset;
}

void Effect::SetColorMultiplier(Formats::Rgba8Color color)
{
    colorMultiplier = color;
}

std::optional<Formats::Rgba8Color> Effect::ColorMultiplier() const
{
    return colorMultiplier;
}

void Effect::SetSizeMultiplier(Formats::Vector2 multiplier)
{
    sizeMultiplier = multiplier;
}

Formats::Vector2 Effect::SizeMultiplier() const
{
    return sizeMultiplier;
}

void Effect::SetParticleLife(std::uint16_t steps)
{
    for (std::unique_ptr<Emitter>& emitter : emitters)
        emitter->Settings().particleLife = steps;
}

void Effect::SetRadiationSpeed(float speed)
{
    for (std::unique_ptr<Emitter>& emitter : emitters)
        emitter->Settings().radiationSpeed = speed;
}

void Effect::SetInitialVelocityRandomPercent(std::uint8_t percent)
{
    for (std::unique_ptr<Emitter>& emitter : emitters)
        emitter->Settings().initialVelocityRandomPercent = percent;
}

// The steps an emitter runs at once when it is made (its pre-roll) come in the effect's first step.
void Effect::Update()
{
    Step();

    bool needsMoreSteps = true;
    while (needsMoreSteps)
    {
        needsMoreSteps = false;
        for (std::unique_ptr<Emitter>& emitter : emitters)
        {
            if (!emitter->NeedsPreRoll())
                continue;
            emitter->CountPreRollStep();
            needsMoreSteps = true;
        }
        if (needsMoreSteps)
            Step();
    }
}

void Effect::Step()
{
    for (std::size_t index = 0; index < emitters.size(); ++index)
        emitters[index]->Step(childQueue);
    MakeChildren();
}

void Effect::MakeChildren()
{
    for (const ChildBirth& birth : childQueue)
    {
        const EmitterResource* resource = resources.FindEmitter(birth.emitterName);
        if (resource == nullptr)
            continue;

        std::unique_ptr<Emitter> child = MakeEmitter(*resource, birth.request.drawPriority);
        child->SetParentMatrix(birth.parentMatrix);

        const Formats::ChildRequest& request = birth.request;
        bool inheritsSomething = request.velocityPercent != 0 || request.sizePercent != 0 || request.alphaPercent != 0 ||
            request.colorPercent != 0;
        if (inheritsSomething)
            child->SetInheritance({request, birth.source, birth.sourceHeightRule});
        emitters.push_back(std::move(child));
    }
    childQueue.clear();
}

void Effect::Stop()
{
    for (std::unique_ptr<Emitter>& emitter : emitters)
        emitter->Stop();
}

bool Effect::IsFinished() const
{
    if (!childQueue.empty())
        return false;

    for (const std::unique_ptr<Emitter>& emitter : emitters)
    {
        if (emitter->IsEmitting() || emitter->HasParticles())
            return false;
    }
    return true;
}

std::vector<const Emitter*> Effect::EmittersToDraw() const
{
    std::vector<const Emitter*> ordered;
    for (const std::unique_ptr<Emitter>& emitter : emitters)
        ordered.push_back(emitter.get());

    std::stable_sort(ordered.begin(), ordered.end(),
        [](const Emitter* first, const Emitter* second) { return first->DrawPriority() < second->DrawPriority(); });
    return ordered;
}

EffectResources& Effect::Resources() const
{
    return resources;
}

}
