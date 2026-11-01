#pragma once

#include "Effects/EffectResources.h"
#include "Effects/Emitter.h"

#include "Graphics/Matrix4.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Effects
{

// One playing effect: the emitter of its name and the child emitters its particles make. A program places it with a matrix and may
// change a few of its settings, as the channels do.
class Effect
{
public:
    // Throws an EffectError when no loaded file has the emitter, or when it needs a part of the library that WeeOS does not build.
    // The resources must outlive the effect.
    Effect(EffectResources& effectResources, std::string_view emitterName, std::uint16_t randomSeed);

    // Where the effect stands in the world: the space of its first emitter.
    void SetMatrix(const Graphics::Matrix4& matrix);
    void SetTranslation(Formats::Vector3 translation);

    // An angle in radians added to the rotation of every particle picture, as the channels turn their hands with the remote.
    void SetRotationOffset(Formats::Vector3 radians);
    Formats::Vector3 RotationOffset() const;

    // Every color of every particle is multiplied by this color.
    void SetColorMultiplier(Formats::Rgba8Color color);
    std::optional<Formats::Rgba8Color> ColorMultiplier() const;

    // The width and height of every particle picture are multiplied by these.
    void SetSizeMultiplier(Formats::Vector2 multiplier);
    Formats::Vector2 SizeMultiplier() const;

    // Changes of the start settings of every emitter, made before the first step (the Wii Speak Channel's ripples).
    void SetParticleLife(std::uint16_t steps);
    void SetRadiationSpeed(float speed);
    void SetInitialVelocityRandomPercent(std::uint8_t percent);

    // One step of 1/60 of a second.
    void Update();

    // No new particles are made; the effect ends when the last one is gone.
    void Stop();

    // True when no emitter makes particles any more and no particle is left.
    bool IsFinished() const;

    // In the order they are drawn: by their draw priority, lower first.
    std::vector<const Emitter*> EmittersToDraw() const;

    EffectResources& Resources() const;

private:
    void Step();
    void MakeChildren();
    std::unique_ptr<Emitter> MakeEmitter(const Formats::EmitterResource& resource, std::uint8_t priority);

    EffectResources& resources;
    EffectRandom random;
    Graphics::Matrix4 matrix;
    std::vector<std::unique_ptr<Emitter>> emitters;
    std::vector<ChildBirth> childQueue;
    Formats::Vector3 rotationOffset = {0, 0, 0};
    std::optional<Formats::Rgba8Color> colorMultiplier;
    Formats::Vector2 sizeMultiplier = {1, 1};
};

// The draw priority of an effect's first emitter; a child takes the priority its key asks for.
constexpr std::uint8_t FirstEmitterPriority = 0x80;

// Throws an EffectError when the emitter needs a part of the effect library that WeeOS does not build.
void CheckEmitterIsBuilt(const Formats::EmitterResource& resource);

}
