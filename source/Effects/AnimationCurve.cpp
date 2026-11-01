#include "Effects/AnimationCurve.h"

#include "Effects/EffectRandom.h"

#include <algorithm>
#include <cmath>

namespace Effects
{

using Formats::AnimationFlag::FitsToLife;
using Formats::AnimationFlag::LoopsForever;
using Formats::AnimationFlag::MirrorsOddLoops;

namespace
{

constexpr std::uint8_t InterpolationMask = 0x3;
constexpr std::uint8_t LinearInterpolation = 0;
constexpr std::uint8_t SmoothInterpolation = 1;
constexpr std::uint8_t StepInterpolation = 2;

// Version 11's eased curves: bit 3 eases the start, bit 2 the end, each with a slope of 1.5.
constexpr std::uint8_t EasesTheStart = 0x08;
constexpr std::uint8_t EasesTheEnd = 0x04;
constexpr float EaseSlope = 1.5f;

constexpr int RandomBitsShift = 16;
constexpr std::uint32_t SignBit = 0x10000;
constexpr int SignedStepShift = 15;
constexpr float HighestByte = 255;

bool IsOdd(std::uint32_t number)
{
    return number % 2 == 1;
}

CurveTime TimeWithoutLoops(const Formats::EffectAnimation& animation, std::uint32_t age, std::uint32_t life, float lastFrame)
{
    CurveTime time;
    if ((animation.flags & FitsToLife) == 0)
    {
        time.frame = std::min(static_cast<float>(age), lastFrame);
        return time;
    }

    if (life <= 1)
    {
        time.frame = lastFrame;
        return time;
    }

    time.frame = std::min(age * lastFrame / (life - 1), lastFrame);
    return time;
}

CurveTime TimeOfLoopsByFrames(const Formats::EffectAnimation& animation, std::uint32_t age, std::uint32_t lastFrame)
{
    bool loopsForever = (animation.flags & LoopsForever) != 0;
    bool mirrors = (animation.flags & MirrorsOddLoops) != 0;

    CurveTime time;
    time.loop = age / lastFrame;
    bool isPastTheLastLoop = !loopsForever && time.loop >= animation.loopCount;
    if (isPastTheLastLoop)
    {
        time.loop = animation.loopCount - 1u;
        bool endsMirrored = mirrors && !IsOdd(animation.loopCount);
        time.frame = endsMirrored ? 0.0f : static_cast<float>(lastFrame);
        return time;
    }

    std::uint32_t frame = age - time.loop * lastFrame;
    if (mirrors && IsOdd(time.loop))
        frame = lastFrame - frame;
    time.frame = static_cast<float>(frame);
    return time;
}

CurveTime TimeOfLoopsOverTheLife(const Formats::EffectAnimation& animation, std::uint32_t age, std::uint32_t life, float lastFrame)
{
    bool mirrors = (animation.flags & MirrorsOddLoops) != 0;

    CurveTime time;
    if (life <= 1 || age >= life - 1)
    {
        time.loop = animation.loopCount - 1u;
        bool endsMirrored = mirrors && !IsOdd(animation.loopCount);
        time.frame = endsMirrored ? 0.0f : lastFrame;
        return time;
    }

    float framesPerStep = animation.loopCount * lastFrame / (life - 1);
    float position = age * framesPerStep;
    time.loop = static_cast<std::uint32_t>(position / lastFrame);
    time.frame = position - time.loop * lastFrame;
    if (mirrors && IsOdd(time.loop))
        time.frame = lastFrame - time.frame;
    return time;
}

std::uint32_t PickEntry(std::uint32_t& random, std::size_t entryCount)
{
    std::uint32_t entry = (random >> RandomBitsShift) % static_cast<std::uint32_t>(entryCount);
    random = NextRandomState(random);
    return entry;
}

float RandomByte(float base, float step, std::uint32_t random)
{
    std::int16_t signedRandom = static_cast<std::int16_t>(random >> RandomBitsShift);
    int offset = static_cast<int>(step) * signedRandom;
    float value = base + static_cast<float>(offset >> SignedStepShift);
    return std::clamp(value, 0.0f, HighestByte);
}

std::vector<float> RandomKeyValues(const Formats::EffectAnimation& animation, const Formats::AnimationKey& key, std::uint16_t particleSeed,
    std::uint32_t loop)
{
    std::uint32_t random = KeyRandomNumber(particleSeed, animation.randomSeed, loop, key.rangeIndex);

    const Formats::RandomRange* range = nullptr;
    bool fromRandomTable = (key.randomFlags & Formats::KeyRandomFlag::FromRandomTable) != 0;
    if (fromRandomTable && !animation.randomTable.empty())
        range = &animation.randomTable[PickEntry(random, animation.randomTable.size())];
    if (!fromRandomTable && key.rangeIndex < animation.rangeTable.size())
        range = &animation.rangeTable[key.rangeIndex];
    if (range == nullptr)
        return key.values;

    bool isNegative = false;
    if (range->hasRandomSign)
    {
        isNegative = (random & SignBit) == 0;
        random = NextRandomState(random);
    }

    bool oneRandomForAll = (animation.flags & Formats::AnimationFlag::OneRandomForAllComponents) != 0;
    std::vector<float> values;
    for (std::size_t component = 0; component < range->bases.size(); ++component)
    {
        float value = 0;
        if (animation.kind == Formats::AnimationValueKind::Bytes)
            value = RandomByte(range->bases[component], range->steps[component], random);
        else
            value = range->bases[component] + range->steps[component] * static_cast<float>(random >> RandomBitsShift);

        values.push_back(isNegative ? -value : value);
        if (!oneRandomForAll)
            random = NextRandomState(random);
    }
    return values;
}

std::vector<float> ValuesOfKey(const Formats::EffectAnimation& animation, const Formats::AnimationKey& key, std::uint16_t particleSeed,
    std::uint32_t loop)
{
    if (key.randomFlags == 0)
        return key.values;

    return RandomKeyValues(animation, key, particleSeed, loop);
}

float EaseVersion11(std::uint8_t interpolation, float t)
{
    if (interpolation == SmoothInterpolation)
        return t * t * (3 - 2 * t);

    float startSlope = (interpolation & EasesTheStart) != 0 ? EaseSlope : 0.0f;
    float endSlope = (interpolation & EasesTheEnd) != 0 ? EaseSlope : 0.0f;
    return t * (startSlope + t * (3 - 2 * startSlope - endSlope) + t * t * (startSlope + endSlope - 2));
}

// How far from the first key towards the second the value is, for a fraction t of the way between them.
float Ease(std::uint8_t interpolation, float t, std::uint16_t version)
{
    std::uint8_t mode = interpolation & InterpolationMask;
    if (mode == LinearInterpolation)
        return t;
    if (mode == StepInterpolation)
        return 0;
    if (mode != SmoothInterpolation)
        return 0;

    if (version == Formats::EffectFileVersion5)
        return t * t * (3 - 2 * t);
    return EaseVersion11(interpolation, t);
}

std::vector<float> BakedValues(const Formats::EffectAnimation& animation, float frame)
{
    std::size_t lastFrame = animation.frameCount > 0 ? animation.frameCount - 1u : 0;
    std::size_t frameNumber = std::min(static_cast<std::size_t>(frame), lastFrame);
    auto first = animation.bakedValues.begin() + static_cast<std::ptrdiff_t>(frameNumber * animation.componentCount);
    return {first, first + static_cast<std::ptrdiff_t>(animation.componentCount)};
}

std::size_t IndexOfKeyAtOrBefore(const Formats::EffectAnimation& animation, float frame)
{
    std::size_t found = 0;
    for (std::size_t index = 0; index < animation.keys.size(); ++index)
    {
        if (animation.keys[index].frame <= frame)
            found = index;
    }
    return found;
}

}

CurveTime FindCurveTime(const Formats::EffectAnimation& animation, std::uint32_t age, std::uint32_t life, std::uint16_t version)
{
    CurveTime time;
    if (animation.frameCount < 2)
    {
        time.loop = age;
        return time;
    }

    std::uint32_t lastFrame = animation.frameCount - 1u;
    bool loops = (animation.flags & LoopsForever) != 0 || animation.loopCount >= 2;
    bool fitsToLife = (animation.flags & FitsToLife) != 0;

    if (!loops)
        time = TimeWithoutLoops(animation, age, life, static_cast<float>(lastFrame));
    else if (!fitsToLife)
        time = TimeOfLoopsByFrames(animation, age, lastFrame);
    else
        time = TimeOfLoopsOverTheLife(animation, age, life, static_cast<float>(lastFrame));

    if (version == Formats::EffectFileVersion5)
        time.frame = std::floor(time.frame);
    return time;
}

std::vector<float> EvaluateCurve(const Formats::EffectAnimation& animation, const CurveTime& time, std::uint16_t particleSeed,
    std::uint16_t version)
{
    if (animation.isBaked)
        return BakedValues(animation, time.frame);
    if (animation.keys.empty())
        return {};

    std::size_t index = IndexOfKeyAtOrBefore(animation, time.frame);
    const Formats::AnimationKey& key = animation.keys[index];
    std::vector<float> values = ValuesOfKey(animation, key, particleSeed, time.loop);

    bool isBeforeTheFirstKey = time.frame < key.frame;
    bool isTheLastKey = index + 1 == animation.keys.size();
    if (isBeforeTheFirstKey || isTheLastKey || time.frame == key.frame)
        return values;

    const Formats::AnimationKey& nextKey = animation.keys[index + 1];
    std::vector<float> nextValues = ValuesOfKey(animation, nextKey, particleSeed, time.loop);
    float t = (time.frame - key.frame) / static_cast<float>(nextKey.frame - key.frame);
    float eased = Ease(key.interpolation, t, version);
    for (std::size_t component = 0; component < values.size() && component < nextValues.size(); ++component)
        values[component] += (nextValues[component] - values[component]) * eased;
    return values;
}

const Formats::AnimationKey* FindKeyAtOrBefore(const Formats::EffectAnimation& animation, float frame)
{
    if (animation.keys.empty())
        return nullptr;

    return &animation.keys[IndexOfKeyAtOrBefore(animation, frame)];
}

}
