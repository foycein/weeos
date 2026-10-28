#pragma once

#include "Formats/LayoutAnimation.h"

#include <span>

namespace Layout
{

// The key frames of a curve must be sorted by frame. A curve needs at least one key frame, or a LayoutError is thrown.
//
// Before the first key frame the curve has the value of the first key, and after the last key frame the value of the last key.
// A frame that is closer than a thousandth to a key frame gets the value of that key.
// When two keys have the same frame, the curve jumps from the first value to the second one.

// Goes smoothly from key to key. Each key has a slope, which is how steeply the value changes at that key.
float EvaluateHermiteCurve(std::span<const Formats::KeyFrame> keyFrames, float frame);

// Keeps the value of a key until the next key is reached.
float EvaluateStepCurve(std::span<const Formats::KeyFrame> keyFrames, float frame);

// Uses the kind of curve that the target says.
float EvaluateCurve(const Formats::AnimationTarget& target, float frame);

}
