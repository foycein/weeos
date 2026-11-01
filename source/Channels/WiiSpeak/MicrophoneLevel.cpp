#include "Channels/WiiSpeak/MicrophoneLevel.h"

#include <algorithm>
#include <cmath>

namespace Channels
{

namespace
{

constexpr double FullScale = 32768.0;
constexpr float DecibelsPerAmplitudeDecade = 20.0f;

}

float HeardLevelInDecibels(std::span<const std::int16_t> samples)
{
    double sum = 0.0;
    for (std::int16_t sample : samples)
        sum += std::abs(double(sample));

    if (sum == 0.0)
        return SilenceLevel;

    double meanSize = sum / double(samples.size()) / FullScale;
    float levelInDecibels = float(DecibelsPerAmplitudeDecade * std::log10(meanSize));
    return std::max(SilenceLevel, levelInDecibels);
}

float MeterFrame(float levelInDecibels)
{
    float fraction = (levelInDecibels - QuietestMeterLevel) / -QuietestMeterLevel;
    float fractionOnTheMeter = std::clamp(fraction, 0.0f, 1.0f);
    return std::round(fractionOnTheMeter * MeterBarCount);
}

bool IsVoiceHeard(float levelInDecibels)
{
    return levelInDecibels > VoiceLevel;
}

}
