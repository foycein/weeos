#pragma once

#include "Effects/EffectRandom.h"
#include "Effects/Particle.h"

#include "Graphics/Matrix4.h"

#include "Formats/EffectFile.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Effects
{

// A child emitter that a particle asks for (an animation of target Child): made at the end of the effect's step.
struct ChildBirth
{
    std::string_view emitterName;
    Formats::ChildRequest request;
    Particle source;
    Graphics::Matrix4 parentMatrix;
    Formats::ParticleHeightRule sourceHeightRule = Formats::ParticleHeightRule::SizeYTimesScaleY;
};

// What a child takes over from the particle that made it, given to every particle it makes.
struct ChildInheritance
{
    Formats::ChildRequest request;
    Particle source;
    Formats::ParticleHeightRule sourceHeightRule = Formats::ParticleHeightRule::SizeYTimesScaleY;
};

// One emitter of an effect: it makes particles at the place of its form and moves them, step by step, as its resource says.
class Emitter
{
public:
    Emitter(const Formats::EmitterResource& emitterResource, std::uint16_t randomSeed, std::uint8_t priority);

    const Formats::EmitterResource& Resource() const;

    // The settings as they are now: the resource's, with the program's changes and the emitter animations.
    Formats::EmitterSettings& Settings();
    const Formats::EmitterSettings& Settings() const;

    // The space the emitter stands in: the effect's matrix for the first emitter, the place of the particle for a child.
    void SetParentMatrix(const Graphics::Matrix4& matrix);

    void SetInheritance(const ChildInheritance& inheritance);

    // From the emitter's own space into the world: parent * move * turn * scale.
    Graphics::Matrix4 WorldMatrix() const;

    // The scale and turn of the world matrix that the particles take over (EmitterFlag::ParticlesInheritScale and
    // ParticlesInheritRotation), without the move. Their pictures are drawn through it.
    Graphics::Matrix4 ParticleSpaceMatrix() const;

    // One step: the emitter animations, the particles' move, the new particles. Children that particles ask for are added to the queue.
    void Step(std::vector<ChildBirth>& childQueue);

    // No more particles are made; those that live go on.
    void Stop();

    bool IsEmitting() const;
    bool HasParticles() const;
    bool NeedsPreRoll() const;
    void CountPreRollStep();

    // Oldest first.
    const std::vector<Particle>& Particles() const;

    std::uint8_t DrawPriority() const;

private:
    void ApplyEmitterAnimations();
    void CheckLife();
    void UpdateParticles(std::size_t firstParticle, std::vector<ChildBirth>& childQueue);
    bool UpdateParticle(Particle& particle, std::vector<ChildBirth>& childQueue);
    void FireChildren(const Formats::EffectAnimation& animation, const Particle& particle, std::uint32_t curveAge, std::uint32_t curveLife,
        std::vector<ChildBirth>& childQueue) const;
    void Emit();
    float EmitAmount();
    void MakeParticles(std::uint32_t count);
    Particle NewParticle(Formats::Vector3 localPosition, Formats::Vector3 localVelocity);
    std::uint32_t RandomParticleLife();
    void RandomizeSize(Particle& particle);
    Formats::Vector3 RandomRotationOffset();
    void InheritFromTheSource(Particle& particle) const;

    const Formats::EmitterResource& resource;
    Formats::EmitterSettings settings;
    Graphics::Matrix4 parentMatrix;
    std::optional<ChildInheritance> inheritance;
    std::vector<Particle> particles;
    EffectRandom random;
    std::uint8_t drawPriority;

    std::uint32_t age = 0;
    std::uint32_t startDelay = 0;
    std::uint32_t intervalCounter = 0;
    float emitAccumulator = 0;
    bool isFirstEmission = true;
    bool isEmitting = true;
    std::uint32_t preRollStepsLeft = 0;
};

}
