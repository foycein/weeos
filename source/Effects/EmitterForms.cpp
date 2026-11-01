#include "Effects/EmitterForms.h"

#include <cmath>
#include <numbers>

namespace Effects
{

using Formats::EmitterForm;
using Formats::EmitterSettings;
namespace EmitFlag = Formats::EmitFlag;

namespace
{

constexpr float FullTurn = 2 * std::numbers::pi_v<float>;
constexpr float PercentPerUnit = 100;
constexpr float SmallestRadius = 1.19e-7f;
constexpr float FullCircleTolerance = 0.001f;

// The point form spreads its directions over the sphere with a curve that puts fewer of them near the poles of the X axis.
constexpr float PointSpreadBase = 0.66f;
constexpr float PointSpreadGrowth = 0.34f;

enum FormParameter
{
    RadiusAlongX = 0,
    InnerRadiusPercent = 1,
    StartAngle = 2,
    EndAngle = 3,
    DiscRadiusZ = 4,
    CylinderHeight = 4,
    CylinderRadiusZ = 5,
};

bool HasFlag(const EmitterSettings& settings, std::uint32_t flag)
{
    return (settings.emitFlags & flag) != 0;
}

// From the middle out: how far along the radius a particle lies, between the inner radius and the rim.
float RadiusFraction(const EmitterSettings& settings, EffectRandom& random)
{
    float inner = settings.formParameters[InnerRadiusPercent] / PercentPerUnit;
    float fraction = random.NextFraction();
    if (HasFlag(settings, EmitFlag::EvenOverTheArea))
        return std::sqrt(fraction + (1 - fraction) * inner * inner);

    return fraction + inner * (1 - fraction);
}

bool IsFullCircle(float span)
{
    float rest = std::fmod(std::abs(span), FullTurn);
    return rest < FullCircleTolerance || FullTurn - rest < FullCircleTolerance;
}

// The angles of one emission: evenly spaced with a fixed count, random otherwise.
std::vector<float> RingAngles(const EmitterSettings& settings, std::uint32_t count, EffectRandom& random)
{
    float span = settings.formParameters[EndAngle] - settings.formParameters[StartAngle];
    float start = settings.formParameters[StartAngle];
    if (!HasFlag(settings, EmitFlag::FixedStartAngle))
        start = FullTurn * random.NextFraction();

    std::vector<float> angles;
    if (!HasFlag(settings, EmitFlag::FixedCount))
    {
        for (std::uint32_t number = 0; number < count; ++number)
            angles.push_back(start + span * random.NextFraction());
        return angles;
    }

    float step = 0;
    if (IsFullCircle(span))
        step = span / static_cast<float>(count);
    else if (count > 1)
        step = span / static_cast<float>(count - 1);

    for (std::uint32_t number = 0; number < count; ++number)
        angles.push_back(start + step * static_cast<float>(number));
    return angles;
}

float RadiusX(const EmitterSettings& settings)
{
    float radius = settings.formParameters[RadiusAlongX];
    if (std::abs(radius) < SmallestRadius)
        return SmallestRadius;
    return radius;
}

float RadiusZ(const EmitterSettings& settings, FormParameter parameter)
{
    if (HasFlag(settings, EmitFlag::SameSizeOnEveryAxis))
        return RadiusX(settings);
    return settings.formParameters[parameter];
}

FormPlacement PlaceOnPoint(EffectRandom& random)
{
    float signedFraction = 2 * random.NextFraction() - 1;
    float spread = signedFraction * (PointSpreadBase + PointSpreadGrowth * std::abs(signedFraction));
    float ring = std::sqrt(std::max(0.0f, 1 - spread * spread));
    float angle = FullTurn * random.NextFraction();

    FormPlacement placement;
    placement.radiationDirection = {spread, ring * std::cos(angle), ring * std::sin(angle)};
    placement.normalDirection = placement.radiationDirection;
    placement.yAxisDirection = NormalizeOr({spread, 0, ring * std::sin(angle)}, {0, 1, 0});
    return placement;
}

FormPlacement PlaceOnDisc(const EmitterSettings& settings, float angle, EffectRandom& random)
{
    float fraction = RadiusFraction(settings, random);
    float sine = std::sin(angle);
    float cosine = std::cos(angle);

    FormPlacement placement;
    placement.position = {RadiusX(settings) * sine * fraction, 0, -RadiusZ(settings, DiscRadiusZ) * cosine * fraction};
    placement.radiationDirection = {sine, 0, -cosine};
    placement.yAxisDirection = placement.radiationDirection;

    float diffusion = settings.normalDiffusion * fraction;
    if (settings.normalDiffusion != 0)
        placement.normalDirection = {sine * std::sin(diffusion), std::cos(diffusion), -cosine * std::sin(diffusion)};
    return placement;
}

FormPlacement PlaceOnCylinder(const EmitterSettings& settings, float angle, EffectRandom& random)
{
    float fraction = RadiusFraction(settings, random);
    float sine = std::sin(angle);
    float cosine = std::cos(angle);
    float height = settings.formParameters[CylinderHeight] * (2 * random.NextFraction() - 1);

    FormPlacement placement;
    placement.position = {RadiusX(settings) * sine * fraction, height, -RadiusZ(settings, CylinderRadiusZ) * cosine * fraction};
    Vector3 across = {placement.position.x, 0, placement.position.z};
    placement.radiationDirection = NormalizeOr(across, across);
    placement.yAxisDirection = NormalizeOr(placement.position, placement.position);
    placement.normalDirection = {sine, 0, -cosine};
    return placement;
}

Vector3 RandomDirection(EffectRandom& random)
{
    float first = FullTurn * random.NextFraction();
    float second = FullTurn * random.NextFraction();
    float third = FullTurn * random.NextFraction();

    float x = std::cos(third) * std::cos(first) * std::sin(second) + std::sin(first) * std::sin(third);
    float y = std::sin(third) * std::cos(first) * std::sin(second) - std::sin(first) * std::cos(third);
    float z = std::cos(second) * std::cos(first);
    return {x, y, z};
}

Vector3 SpecialDirection(const EmitterSettings& settings, EffectRandom& random)
{
    if (settings.specialDirectionDiffusion == 0)
        return UpTurnedBy(settings.specialDirectionAngles);

    float spread = settings.specialDirectionDiffusion * random.NextFraction();
    float around = FullTurn * random.NextFraction();
    Vector3 diffused = UpTurnedBy({spread, around, 0});
    return Graphics::TransformDirection(MakeRotationXyz(settings.specialDirectionAngles), diffused);
}

}

bool IsFormBuilt(const EmitterSettings& settings)
{
    switch (settings.form)
    {
    case EmitterForm::Point:
    case EmitterForm::Disc:
        return true;
    case EmitterForm::Cylinder:
        return !HasFlag(settings, EmitFlag::FixedCount);
    default:
        return false;
    }
}

std::vector<FormPlacement> PlaceParticles(const EmitterSettings& settings, std::uint32_t count, EffectRandom& random)
{
    std::vector<FormPlacement> placements;
    if (settings.form == EmitterForm::Point)
    {
        for (std::uint32_t number = 0; number < count; ++number)
            placements.push_back(PlaceOnPoint(random));
        return placements;
    }

    for (float angle : RingAngles(settings, count, random))
    {
        if (settings.form == EmitterForm::Cylinder)
            placements.push_back(PlaceOnCylinder(settings, angle, random));
        else
            placements.push_back(PlaceOnDisc(settings, angle, random));
    }
    return placements;
}

Vector3 InitialVelocity(const EmitterSettings& settings, const FormPlacement& placement, EffectRandom& random)
{
    Vector3 velocity = Multiply(placement.radiationDirection, settings.radiationSpeed);
    velocity = Add(velocity, Multiply(placement.yAxisDirection, settings.yAxisSpeed));
    velocity = Add(velocity, Multiply(placement.normalDirection, settings.normalSpeed));

    if (settings.randomDirectionSpeed != 0)
        velocity = Add(velocity, Multiply(RandomDirection(random), settings.randomDirectionSpeed));
    if (settings.specialDirectionSpeed != 0)
        velocity = Add(velocity, Multiply(SpecialDirection(settings, random), settings.specialDirectionSpeed));

    if (settings.initialVelocityRandomPercent != 0)
    {
        float change = settings.initialVelocityRandomPercent / PercentPerUnit * (2 * random.NextFraction() - 1);
        velocity = Multiply(velocity, 1 - change);
    }
    return velocity;
}

}
