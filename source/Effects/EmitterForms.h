#pragma once

#include "Effects/EffectRandom.h"
#include "Effects/VectorMath.h"

#include "Formats/EffectFile.h"

#include <vector>

namespace Effects
{

// Where a new particle starts, in the emitter's own space, and the directions its speeds push it in.
struct FormPlacement
{
    Vector3 position = {0, 0, 0};
    Vector3 radiationDirection = {0, 0, 0};
    Vector3 yAxisDirection = {0, 0, 0};
    Vector3 normalDirection = {0, 1, 0};
};

// True for the forms that WeeOS builds: the point, the disc and the cylinder with random places. The others (line, cube, sphere,
// torus, and the cylinder with even steps) are only used by effects of WeeOS's own server (the Wii Speak lobby).
bool IsFormBuilt(const Formats::EmitterSettings& settings);

// The places of the particles of one emission.
std::vector<FormPlacement> PlaceParticles(const Formats::EmitterSettings& settings, std::uint32_t count, EffectRandom& random);

// The start velocity of a particle: the sum of the emitter's speeds along the directions of its place, along a random direction and
// along the special direction, then made up to a random percentage faster or slower.
Vector3 InitialVelocity(const Formats::EmitterSettings& settings, const FormPlacement& placement, EffectRandom& random);

}
