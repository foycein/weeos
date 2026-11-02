#include "Channels/MiiChannel/MiiFootsteps.h"

#include <algorithm>
#include <cmath>

namespace Channels
{

namespace
{

constexpr std::string_view LeftFootSound = "SEQ_WALK_L";
constexpr std::string_view RightFootSound = "SEQ_WALK_R";

// Where the steps are heard from and how they fade (800258D4, 803BE494, 803BE544, 803BE55C).
constexpr float ListeningPointNearer = 150.0f;
constexpr float SilentDistance = 1000.0f;
constexpr float QuieterPerZoomLevel = 0.25f;

// A step this far to the side is heard fully on that side, times the factor of the zoom level of the camera (80024124, 803BE4F0 to
// 803BE510).
constexpr float FullPanDistance = 800.0f;
constexpr std::array<float, MiiPlaza::FarthestZoomLevel + 1> FullPanFactorOfZoomLevel = {0.46666667f, 1.0f, 1.8f, 4.1333333f};

// The size of a Mii is its height times its weight; the steps change between the smallest size 0 and the largest 127 x 127
// (80018F3C, 803BE230 to 803BE288).
constexpr float LargestSize = 16129.0f;
constexpr float PitchOfTheSmallest = 1.2f;
constexpr float PitchOfTheLargest = 0.7f;
constexpr float VolumeOfTheSmallest = 0.2f;
constexpr float VolumeOfTheLargest = 1.0f;
constexpr float PriorityOfTheSmallest = 10.0f;
constexpr float PriorityOfTheLargest = 120.0f;
constexpr float WomanPitchRaised = 0.5f;
constexpr float ManPitchLowered = 0.05f;

// The last byte of the Mii id, modulo 10, is the id step of the Mii: every id step raises the pitch by a 120th and lowers the volume by a
// 70th.
constexpr std::uint32_t LastByteOfId = 0xFF;
constexpr std::uint32_t IdSteps = 10;
constexpr float PitchRaisedPerIdStep = 1.0f / 120.0f;
constexpr float VolumeLoweredPerIdStep = 1.0f / 70.0f;
constexpr float LoudestStep = 0.9f;

// A Mii of the parade that runs (its pace 1.517 is above 1.2 and not above 2).
constexpr float RunningVolumeFactor = 1.4f;
constexpr float RunningPitchFactor = 1.21f;
constexpr float LoudestRunningStep = 0.98f;

// The lowest pitch of every band from the second on.
constexpr std::array<float, PitchBandCount - 1> PitchBandStarts = {0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f};

// A band is heard again 12 steps after it was heard, and 3 steps after a band next to it.
constexpr int StepsBetweenStepsOfABand = 12;
constexpr int StepsBetweenStepsOfNeighbourBands = 3;

// From 0 for the smallest Mii to 1 for the largest.
float SizeOf(const Mii::MiiRecord& record)
{
    float size = static_cast<float>(record.height * record.weight);
    return std::min(size / LargestSize, 1.0f);
}

float Between(float smallestValue, float largestValue, float size)
{
    return smallestValue + (largestValue - smallestValue) * size;
}

float IdStepOf(const Mii::MiiRecord& record)
{
    std::uint32_t lastByte = record.miiId & LastByteOfId;
    return static_cast<float>(lastByte % IdSteps);
}

}

FootstepPlace ParadeFootstepPlace(Formats::Vector2 groundPosition, const MiiPlaza& parade)
{
    Formats::Vector2 listeningPoint = parade.GroundPointAhead();
    listeningPoint.y += ListeningPointNearer;

    float sideDistance = groundPosition.x - listeningPoint.x;
    float distance = std::hypot(sideDistance, groundPosition.y - listeningPoint.y);
    int zoomLevel = parade.Camera().zoomLevel;
    float zoomLoudness = 1.0f - static_cast<float>(zoomLevel) * QuieterPerZoomLevel;
    float loudness = (1.0f - distance / SilentDistance) * zoomLoudness;
    float fullPanDistance = FullPanDistance * FullPanFactorOfZoomLevel[zoomLevel];

    FootstepPlace place;
    place.loudness = std::max(loudness, 0.0f);
    place.pan = std::clamp(sideDistance / fullPanDistance, -1.0f, 1.0f);
    return place;
}

float PitchOfSteps(const Mii::MiiRecord& record)
{
    float pitch = Between(PitchOfTheSmallest, PitchOfTheLargest, SizeOf(record));
    if (record.gender == Mii::Gender::Female)
        pitch += WomanPitchRaised;
    else
        pitch -= ManPitchLowered;

    return pitch + IdStepOf(record) * PitchRaisedPerIdStep;
}

Sound::SoundStartSettings FootstepSettings(const Mii::MiiRecord& record, FootstepPlace place, bool isRunning)
{
    float size = SizeOf(record);
    float volume = place.loudness * Between(VolumeOfTheSmallest, VolumeOfTheLargest, size);
    volume -= IdStepOf(record) * VolumeLoweredPerIdStep;

    Sound::SoundStartSettings settings;
    settings.volume = std::clamp(volume, 0.0f, LoudestStep);
    settings.pan = place.pan;
    settings.pitch = PitchOfSteps(record);
    settings.playerPriority = static_cast<std::uint8_t>(Between(PriorityOfTheSmallest, PriorityOfTheLargest, size));
    if (isRunning)
    {
        settings.volume = std::min(settings.volume * RunningVolumeFactor, LoudestRunningStep);
        settings.pitch *= RunningPitchFactor;
    }
    return settings;
}

int PitchBandOf(float pitch)
{
    int band = 0;
    while (band < PitchBandCount - 1 && pitch >= PitchBandStarts[band])
        ++band;
    return band;
}

MiiFootsteps::MiiFootsteps()
{
    stepsSinceBandWasHeard.fill(StepsBetweenStepsOfABand);
}

std::optional<Footstep> MiiFootsteps::Step(const PlazaMii& plazaMii, PlazaMoment foot, FootstepPlace place)
{
    int band = PitchBandOf(PitchOfSteps(plazaMii.mii.record));
    if (!MayBeHeard(band))
        return std::nullopt;

    stepsSinceBandWasHeard[band] = 0;
    bool isRunning = plazaMii.activity == PlazaActivity::Running;
    std::string_view soundName = foot == PlazaMoment::StepsWithTheLeftFoot ? LeftFootSound : RightFootSound;
    return Footstep{soundName, FootstepSettings(plazaMii.mii.record, place, isRunning)};
}

void MiiFootsteps::CountStep()
{
    for (int& steps : stepsSinceBandWasHeard)
        ++steps;
}

bool MiiFootsteps::MayBeHeard(int band) const
{
    if (stepsSinceBandWasHeard[band] < StepsBetweenStepsOfABand)
        return false;

    bool hasHigherBand = band + 1 < PitchBandCount;
    if (hasHigherBand && stepsSinceBandWasHeard[band + 1] < StepsBetweenStepsOfNeighbourBands)
        return false;

    bool hasLowerBand = band > 0;
    if (hasLowerBand && stepsSinceBandWasHeard[band - 1] < StepsBetweenStepsOfNeighbourBands)
        return false;

    return true;
}

}
