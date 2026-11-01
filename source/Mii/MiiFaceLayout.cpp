#include "Mii/MiiFaceLayout.h"

#include "Mii/MiiRotationOffsets.h"

#include <cmath>

namespace Mii
{

namespace
{

// The layout is worked out on a face of 64 by 64 and scaled to the texture.
constexpr float LayoutSize = 64.0f;
constexpr float FaceMiddle = 32.0f;

// Every quad is squeezed a little after it is turned; the same width is the step of the horizontal positions.
constexpr float SqueezeWidth = 0.889615f;
constexpr float SqueezeHeight = 0.927667f;
constexpr float HorizontalStep = SqueezeWidth;
constexpr float VerticalStep = 1.076094f;
constexpr float MoleHorizontalStep = 1.779229f;

constexpr float GrowthPerSizeStep = 0.4f;
constexpr float DegreesPerRotationStep = 11.25f;
constexpr float FullTurnDegrees = 360.0f;
constexpr float RadiansPerDegree = 0.017453292f;

constexpr float EyeMiddle = 18.451525f;
constexpr float EyeWidth = 5.34375f;
constexpr float EyeHeight = 4.5f;
constexpr float EyebrowMiddle = 16.549807f;
constexpr float EyebrowWidth = 5.0625f;
constexpr float EyebrowHeight = 4.5f;
constexpr float MouthMiddle = 29.25885f;
constexpr float MouthWidth = 6.1875f;
constexpr float MouthHeight = 4.5f;
constexpr float MustacheMiddle = 31.763554f;
constexpr float MustacheWidth = 4.5f;
constexpr float MustacheHeight = 9.0f;
constexpr float MoleFirstPosition = 17.766165f;
constexpr float MoleMiddle = 17.95986f;

// Where the picture lies around its point: centered, to the left of it, or to the right of it and mirrored.
// Eyes and eyebrows turn around their inner corner, which is the point.
enum class Anchor
{
    Centered,
    LeftOfPoint,
    RightOfPointMirrored,
};

struct QuadPlacement
{
    FaceFeature feature;
    float x;
    float y;
    float width;
    float height;
    float angleDegrees;
    Anchor anchor;
};

float Grow(int size)
{
    return 1.0f + GrowthPerSizeStep * static_cast<float>(size);
}

float AngleOf(int rotation, int offset)
{
    int steps = (rotation + offset) % RotationStepsPerTurn;
    return DegreesPerRotationStep * static_cast<float>(steps);
}

float LeftEdgeOf(Anchor anchor)
{
    if (anchor == Anchor::Centered)
        return -0.5f;
    if (anchor == Anchor::LeftOfPoint)
        return -1.0f;
    return 0.0f;
}

// The picture is scaled to its size, turned (with y downward a positive angle turns clockwise), squeezed and moved to its point.
Formats::Vector2 PlaceCorner(const QuadPlacement& placement, Formats::Vector2 templateCorner, float unit)
{
    float x = templateCorner.x * placement.width * unit;
    float y = templateCorner.y * placement.height * unit;

    float radians = placement.angleDegrees * RadiansPerDegree;
    float turnedX = std::cos(radians) * x - std::sin(radians) * y;
    float turnedY = std::sin(radians) * x + std::cos(radians) * y;

    return {placement.x * unit + SqueezeWidth * turnedX, placement.y * unit + SqueezeHeight * turnedY};
}

FaceFeatureQuad MakeQuad(const QuadPlacement& placement, float unit)
{
    float left = LeftEdgeOf(placement.anchor);
    float right = left + 1.0f;
    float top = -0.5f;
    float bottom = 0.5f;

    float leftS = 0.0f;
    float rightS = 1.0f;
    if (placement.anchor == Anchor::RightOfPointMirrored)
    {
        leftS = 1.0f;
        rightS = 0.0f;
    }

    FaceFeatureQuad quad;
    quad.feature = placement.feature;
    quad.corners = {
        PlaceCorner(placement, {left, top}, unit),
        PlaceCorner(placement, {right, top}, unit),
        PlaceCorner(placement, {right, bottom}, unit),
        PlaceCorner(placement, {left, bottom}, unit),
    };
    quad.textureCoordinates = {{{leftS, 0.0f}, {rightS, 0.0f}, {rightS, 1.0f}, {leftS, 1.0f}}};
    return quad;
}

}

std::vector<FaceFeatureQuad> LayOutFace(const MiiRecord& record, float textureSize)
{
    float unit = textureSize / LayoutSize;

    const MiiEyes& eyes = record.eyes;
    float eyeX = HorizontalStep * static_cast<float>(eyes.spacing);
    float eyeY = EyeMiddle + VerticalStep * static_cast<float>(eyes.vertical);
    float eyeAngle = AngleOf(eyes.rotation, EyeRotationOffset[eyes.type]);

    const MiiEyebrows& eyebrows = record.eyebrows;
    float eyebrowX = HorizontalStep * static_cast<float>(eyebrows.spacing);
    float eyebrowY = EyebrowMiddle + VerticalStep * static_cast<float>(eyebrows.vertical);
    float eyebrowAngle = AngleOf(eyebrows.rotation, EyebrowRotationOffset[eyebrows.type]);

    float mouthY = MouthMiddle + VerticalStep * static_cast<float>(record.mouth.vertical);
    float mouthGrowth = Grow(record.mouth.size);

    float mustacheY = MustacheMiddle + VerticalStep * static_cast<float>(record.facialHair.mustacheVertical);
    float mustacheGrowth = Grow(record.facialHair.mustacheSize);

    float moleX = MoleFirstPosition + MoleHorizontalStep * static_cast<float>(record.mole.horizontal);
    float moleY = MoleMiddle + VerticalStep * static_cast<float>(record.mole.vertical);
    float moleGrowth = Grow(record.mole.size);

    std::vector<QuadPlacement> placements = {
        {FaceFeature::Mustache, FaceMiddle, mustacheY, MustacheWidth * mustacheGrowth, MustacheHeight * mustacheGrowth, 0, Anchor::LeftOfPoint},
        {FaceFeature::Mustache, FaceMiddle, mustacheY, MustacheWidth * mustacheGrowth, MustacheHeight * mustacheGrowth, 0,
            Anchor::RightOfPointMirrored},
        {FaceFeature::Mouth, FaceMiddle, mouthY, MouthWidth * mouthGrowth, MouthHeight * mouthGrowth, 0, Anchor::Centered},
        {FaceFeature::Eyebrow, FaceMiddle - eyebrowX, eyebrowY, EyebrowWidth * Grow(eyebrows.size), EyebrowHeight * Grow(eyebrows.size),
            eyebrowAngle, Anchor::LeftOfPoint},
        {FaceFeature::Eyebrow, FaceMiddle + eyebrowX, eyebrowY, EyebrowWidth * Grow(eyebrows.size), EyebrowHeight * Grow(eyebrows.size),
            FullTurnDegrees - eyebrowAngle, Anchor::RightOfPointMirrored},
        {FaceFeature::Eye, FaceMiddle - eyeX, eyeY, EyeWidth * Grow(eyes.size), EyeHeight * Grow(eyes.size), eyeAngle, Anchor::LeftOfPoint},
        {FaceFeature::Eye, FaceMiddle + eyeX, eyeY, EyeWidth * Grow(eyes.size), EyeHeight * Grow(eyes.size), FullTurnDegrees - eyeAngle,
            Anchor::RightOfPointMirrored},
        {FaceFeature::Mole, moleX, moleY, moleGrowth, moleGrowth, 0, Anchor::Centered},
    };

    std::vector<FaceFeatureQuad> quads;
    for (const QuadPlacement& placement : placements)
        quads.push_back(MakeQuad(placement, unit));
    return quads;
}

}
