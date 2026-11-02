#pragma once

#include "Channels/MiiChannel/MiiPlaza.h"

#include "Mii/MiiRecord.h"
#include "Sound/SoundStartSettings.h"

#include <array>
#include <optional>
#include <string_view>

namespace Channels
{

// How loud a step is where the Mii walks, and where it is heard between the speakers, before the Mii's own size changes it.
struct FootstepPlace
{
    // 1 is full, 0 is silence.
    float loudness = 1.0f;

    // -1 is the left, 0 the middle and 1 the right.
    float pan = 0.0f;
};

// The place of a step on the road of the parade (800258D4 writes it into the fields 0x75C and 0x760 of every Mii). The steps are heard
// from the point of the ground ahead of the camera, 150 units nearer to the camera. A step is quieter the farther it is from there,
// silent from 1000 units on, and a quarter quieter for every zoom level of the camera; it is panned by how far it is to the side, fully
// at 800 units for the zoom level 1 of the parade's camera.
FootstepPlace ParadeFootstepPlace(Formats::Vector2 groundPosition, const MiiPlaza& parade);

// What a step of a Mii sounds like at its place (80018F3C): a small or light Mii steps higher and softer, a large and heavy one lower and
// louder; a woman steps higher; the last byte of the Mii id makes every Mii a little higher and softer than the next. A Mii of the
// parade that runs steps louder and higher.
Sound::SoundStartSettings FootstepSettings(const Mii::MiiRecord& record, FootstepPlace place, bool isRunning);

// The pitch of a Mii's steps when it walks.
float PitchOfSteps(const Mii::MiiRecord& record);

// The steps fall into ten bands by their pitch: below 0.8, then one band for every 0.1, and 1.6 and above.
constexpr int PitchBandCount = 10;
int PitchBandOf(float pitch);

struct Footstep
{
    std::string_view soundName;
    Sound::SoundStartSettings settings;
};

// The steps of the Miis of the parade (8001E448, 80018F3C), where every Mii is heard (800255D0). A Mii that puts its left foot down plays
// SEQ_WALK_L, its right foot SEQ_WALK_R. So that a hundred Miis do not drown everything, a step is heard only when no step of its pitch
// band was heard in the last 12 steps of the channel, and none of the two bands next to it in the last 3.
class MiiFootsteps
{
public:
    MiiFootsteps();

    // The step of a foot of the Mii (StepsWithTheLeftFoot or StepsWithTheRightFoot), or nothing when it is not heard.
    std::optional<Footstep> Step(const PlazaMii& plazaMii, PlazaMoment foot, FootstepPlace place);

    // One step of the channel has passed (80018914).
    void CountStep();

private:
    bool MayBeHeard(int band) const;

    std::array<int, PitchBandCount> stepsSinceBandWasHeard;
};

}
