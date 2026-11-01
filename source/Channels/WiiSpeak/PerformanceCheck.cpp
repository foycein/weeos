#include "Channels/WiiSpeak/PerformanceCheck.h"

#include <algorithm>
#include <cmath>

namespace Channels
{

namespace
{

constexpr float DecibelsPerAmplitudeDecade = 20.0f;

// The delay is searched on this many samples of the heard sound (one second of the microphone); the gain uses all of them.
constexpr std::size_t DelaySearchSamples = 16000;

constexpr float LargestSampleValue = 32767.0f;
constexpr float SmallestSampleValue = -32768.0f;

struct EchoFit
{
    std::size_t delay = 0;
    double gain = 0.0;
};

// How much of the played sound, at this delay, is in the heard sound: the correlation of the two and the energy of the played part.
void Correlate(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t delay, std::size_t count,
    double& correlation, double& energy)
{
    correlation = 0.0;
    energy = 0.0;
    for (std::size_t sample = delay; sample < count; ++sample)
    {
        double playedSample = played[sample - delay];
        correlation += double(heard[sample]) * playedSample;
        energy += playedSample * playedSample;
    }
}

EchoFit FindEcho(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t longestDelay)
{
    std::size_t searchCount = std::min({heard.size(), played.size(), DelaySearchSamples});
    EchoFit best;
    double bestMatch = 0.0;
    for (std::size_t delay = 0; delay <= longestDelay && delay < searchCount; ++delay)
    {
        double correlation = 0.0;
        double energy = 0.0;
        Correlate(heard, played, delay, searchCount, correlation, energy);
        if (energy <= 0.0)
            continue;

        double match = correlation * correlation / energy;
        if (match > bestMatch)
        {
            bestMatch = match;
            best.delay = delay;
        }
    }

    double correlation = 0.0;
    double energy = 0.0;
    Correlate(heard, played, best.delay, std::min(heard.size(), played.size() + best.delay), correlation, energy);
    if (energy > 0.0)
        best.gain = correlation / energy;

    return best;
}

}

CheckResult JudgeLevels(float heardLevel, float reducedLevel)
{
    if (heardLevel < QuietHeardLevel || reducedLevel < QuietReducedLevel)
        return CheckResult::TooQuiet;

    if (heardLevel > LoudHeardLevel)
        return CheckResult::TooLoud;

    float reductionInDecibels = DecibelsPerAmplitudeDecade * std::log10(heardLevel / reducedLevel);
    int score = 0;
    for (float limit : ScoreLimitsInDecibels)
    {
        if (reductionInDecibels >= limit)
            ++score;
    }
    return static_cast<CheckResult>(score);
}

float LevelOf(std::span<const std::int16_t> samples)
{
    if (samples.empty())
        return 0.0f;

    double sum = 0.0;
    for (std::int16_t sample : samples)
        sum += std::abs(double(sample));

    return float(sum / double(samples.size())) * LevelUnitsPerSampleUnit;
}

std::vector<std::int16_t> RemoveEcho(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t longestDelay)
{
    EchoFit echo = FindEcho(heard, played, longestDelay);
    std::vector<std::int16_t> reduced;
    reduced.reserve(heard.size());
    for (std::size_t sample = 0; sample < heard.size(); ++sample)
    {
        double echoSample = 0.0;
        if (sample >= echo.delay && sample - echo.delay < played.size())
            echoSample = echo.gain * played[sample - echo.delay];

        double left = std::round(double(heard[sample]) - echoSample);
        reduced.push_back(static_cast<std::int16_t>(std::clamp(left, double(SmallestSampleValue), double(LargestSampleValue))));
    }
    return reduced;
}

}
