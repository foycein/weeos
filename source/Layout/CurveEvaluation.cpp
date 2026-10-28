#include "Layout/CurveEvaluation.h"

#include "Layout/LayoutError.h"

namespace Layout
{

namespace
{

constexpr float FrameTolerance = 0.001f;

void RequireKeyFrames(std::span<const Formats::KeyFrame> keyFrames)
{
    if (keyFrames.empty())
        throw LayoutError("An animation curve has no key frames");
}

bool IsAtKeyFrame(float frame, const Formats::KeyFrame& keyFrame)
{
    float distance = frame - keyFrame.frame;
    return -FrameTolerance < distance && distance < FrameTolerance;
}

// The curve through the two keys. The position goes from 0 at the lower key to 1 at the upper key,
// and four weights say how much each value and each slope counts at that position.
float InterpolateBetweenKeys(const Formats::KeyFrame& lower, const Formats::KeyFrame& upper, float frame)
{
    float duration = upper.frame - lower.frame;
    float position = (frame - lower.frame) / duration;
    float positionSquared = position * position;
    float positionCubed = positionSquared * position;

    float lowerValueWeight = 2.0f * positionCubed - 3.0f * positionSquared + 1.0f;
    float lowerSlopeWeight = (positionCubed - 2.0f * positionSquared + position) * duration;
    float upperValueWeight = -2.0f * positionCubed + 3.0f * positionSquared;
    float upperSlopeWeight = (positionCubed - positionSquared) * duration;

    return lowerValueWeight * lower.value + lowerSlopeWeight * lower.slope
        + upperValueWeight * upper.value + upperSlopeWeight * upper.slope;
}

}

float EvaluateHermiteCurve(std::span<const Formats::KeyFrame> keyFrames, float frame)
{
    RequireKeyFrames(keyFrames);

    if (keyFrames.size() == 1 || frame <= keyFrames.front().frame)
        return keyFrames.front().value;

    if (frame >= keyFrames.back().frame)
        return keyFrames.back().value;

    // The frame lies between two keys: the upper key is the first one that is not before the frame.
    std::size_t upperIndex = 1;
    while (keyFrames[upperIndex].frame < frame)
        ++upperIndex;

    const Formats::KeyFrame& lower = keyFrames[upperIndex - 1];
    const Formats::KeyFrame& upper = keyFrames[upperIndex];

    if (IsAtKeyFrame(frame, upper))
    {
        bool nextKeyHasTheSameFrame = upperIndex + 1 < keyFrames.size() && keyFrames[upperIndex + 1].frame == upper.frame;
        return nextKeyHasTheSameFrame ? keyFrames[upperIndex + 1].value : upper.value;
    }

    return InterpolateBetweenKeys(lower, upper, frame);
}

float EvaluateStepCurve(std::span<const Formats::KeyFrame> keyFrames, float frame)
{
    RequireKeyFrames(keyFrames);

    if (keyFrames.size() == 1 || frame <= keyFrames.front().frame)
        return keyFrames.front().value;

    if (keyFrames.back().frame <= frame)
        return keyFrames.back().value;

    // The frame lies between two keys: the lower key is the last one that is not after the frame.
    std::size_t lowerIndex = 0;
    while (keyFrames[lowerIndex + 1].frame <= frame)
        ++lowerIndex;

    const Formats::KeyFrame& upper = keyFrames[lowerIndex + 1];
    if (IsAtKeyFrame(frame, upper))
        return upper.value;

    return keyFrames[lowerIndex].value;
}

float EvaluateCurve(const Formats::AnimationTarget& target, float frame)
{
    if (target.keyFrameKind == Formats::KeyFrameKind::Hermite)
        return EvaluateHermiteCurve(target.keyFrames, frame);

    return EvaluateStepCurve(target.keyFrames, frame);
}

}
