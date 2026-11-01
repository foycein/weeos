#pragma once

#include "Formats/EffectFile.h"

#include <cstdint>
#include <vector>

namespace Effects
{

// Where on its curve an animation is for a particle (or an emitter) of some age.
struct CurveTime
{
    // Version 5 uses whole frames; version 11 may stand between two frames of a curve that is fitted to the life.
    float frame = 0;

    // How many times the curve has run before; part of the random numbers of the keys.
    std::uint32_t loop = 0;
};

CurveTime FindCurveTime(const Formats::EffectAnimation& animation, std::uint32_t age, std::uint32_t life, std::uint16_t version);

// The values of an animation of floats, bytes or a rotation at a time on its curve. The seed makes the random keys of one particle.
std::vector<float> EvaluateCurve(const Formats::EffectAnimation& animation, const CurveTime& time, std::uint16_t particleSeed,
    std::uint16_t version);

// The key of a texture pattern or a child animation in effect at a frame: the last one at or before it.
const Formats::AnimationKey* FindKeyAtOrBefore(const Formats::EffectAnimation& animation, float frame);

}
