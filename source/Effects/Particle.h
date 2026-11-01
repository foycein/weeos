#pragma once

#include "Formats/EffectFile.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace Effects
{

// One particle. Positions and velocities are in the space of the world that the effect is placed in.
struct Particle
{
    Formats::Vector3 position = {0, 0, 0};
    Formats::Vector3 previousPosition = {0, 0, 0};
    Formats::Vector3 velocity = {0, 0, 0};

    // The emitter's particle parameters, changed by the animations as the particle ages.
    std::array<Formats::Rgba8Color, Formats::ParticleColorCount> colors = {};
    Formats::Vector2 size = {0, 0};
    Formats::Vector2 scale = {1, 1};
    Formats::Vector3 rotation = {0, 0, 0};
    Formats::Vector3 rotationOffset = {0, 0, 0};
    std::array<Formats::ParticleTextureTransform, Formats::ParticleTextureCount> textureTransforms = {};
    std::array<std::string_view, Formats::ParticleTextureCount> textureNames;
    std::uint16_t textureWraps = 0;
    std::uint8_t textureReverses = 0;
    std::uint8_t firstAlphaReference = 0;
    std::uint8_t secondAlphaReference = 0;

    // In steps. A particle lives while its age is below its life.
    std::uint32_t age = 0;
    std::uint32_t life = 1;

    // Makes the random keys of the animations different for every particle.
    std::uint16_t seed = 0;

    // The steps of the particle are this much longer: 1, or more with the emitter's momentum random.
    float momentum = 1;
};

// The half height of a particle's picture, before the effect's size multiplier: its size and scale as the emitter's rule says.
inline float ParticleHeight(const Particle& particle, Formats::ParticleHeightRule rule)
{
    switch (rule)
    {
    case Formats::ParticleHeightRule::SizeXTimesScaleY: return particle.size.x * particle.scale.y;
    case Formats::ParticleHeightRule::SizeYTimesScaleX: return particle.size.y * particle.scale.x;
    case Formats::ParticleHeightRule::SizeXTimesScaleX: return particle.size.x * particle.scale.x;
    default: return particle.size.y * particle.scale.y;
    }
}

}
