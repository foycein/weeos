#include "Sound/Reverb.h"

#include <algorithm>
#include <cmath>

namespace Sound
{

namespace
{

// The rate at which the original works, and at which its delay lines are measured.
constexpr double OriginalSampleRate = 32000.0;

// The lengths of the delay lines of the late reverb in the original, in samples at 32 000 Hz.
constexpr std::array<int, 3> CombLengths = {1789, 1999, 2333};
constexpr std::array<int, 2> AllPassLengths = {433, 149};
constexpr std::array<int, 2> LastAllPassLengths = {47, 73};

// A comb filter falls by 60 decibels, which is a factor of 10 to the power of -3, in the reverb time.
constexpr float PowerOfTenForSixtyDecibels = -3.0f;

// The weight of the previous sample in the damping filter is at most this, so that the filter never stops the sound.
constexpr float HighestDampingWeight = 0.95f;

// The tail is multiplied by this before the channels are mixed, and the crosstalk is multiplied by the second number.
constexpr float TailGain = 0.6f;
constexpr float CrosstalkScale = 0.5f;

// A sound that has been silent for this many times the reverb time and one second more has left a tail far below what a float can hold.
constexpr float TailTimesToKeep = 5.0f;
constexpr double SecondsOfSafety = 1.0;

constexpr float HighestPreDelaySeconds = 0.1f;
constexpr float HighestTimeSeconds = 10.0f;

std::size_t ScaledLength(int lengthAtOriginalRate, std::uint32_t sampleRate)
{
    double length = double(lengthAtOriginalRate) * double(sampleRate) / OriginalSampleRate;
    return std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(length)));
}

float Limited(float value, float highest)
{
    return std::clamp(value, 0.0f, highest);
}

bool IsSilent(std::span<const float> samples)
{
    return std::all_of(samples.begin(), samples.end(), [](float sample) { return sample == 0.0f; });
}

}

Reverb::DelayLine::DelayLine(std::size_t length)
    : samples(length, 0.0f)
{
}

float Reverb::DelayLine::Oldest() const
{
    return samples[position];
}

void Reverb::DelayLine::Replace(float newest)
{
    samples[position] = newest;
    ++position;
    if (position == samples.size())
        position = 0;
}

float Reverb::DelayLine::Exchange(float newest)
{
    if (samples.empty())
        return newest;

    float oldest = Oldest();
    Replace(newest);
    return oldest;
}

void Reverb::DelayLine::Clear()
{
    std::fill(samples.begin(), samples.end(), 0.0f);
    position = 0;
}

Reverb::Reverb(std::uint32_t sampleRate, const ReverbParameters& wantedParameters, std::size_t inputDelaySamples)
{
    parameters.preDelaySeconds = Limited(wantedParameters.preDelaySeconds, HighestPreDelaySeconds);
    parameters.timeSeconds = Limited(wantedParameters.timeSeconds, HighestTimeSeconds);
    parameters.coloration = Limited(wantedParameters.coloration, 1.0f);
    parameters.damping = Limited(wantedParameters.damping, 1.0f);
    parameters.crosstalk = Limited(wantedParameters.crosstalk, 1.0f);
    parameters.mix = Limited(wantedParameters.mix, 1.0f);

    std::size_t preDelayLength = inputDelaySamples + static_cast<std::size_t>(float(sampleRate) * parameters.preDelaySeconds);
    for (std::size_t channelNumber = 0; channelNumber < ChannelCount; ++channelNumber)
    {
        Channel& channel = channels[channelNumber];
        channel.preDelay = DelayLine(preDelayLength);
        for (std::size_t combNumber = 0; combNumber < CombCount; ++combNumber)
            channel.combs[combNumber] = DelayLine(ScaledLength(CombLengths[combNumber], sampleRate));
        for (std::size_t allPassNumber = 0; allPassNumber < AllPassCount; ++allPassNumber)
            channel.allPasses[allPassNumber] = DelayLine(ScaledLength(AllPassLengths[allPassNumber], sampleRate));
        channel.lastAllPass = DelayLine(ScaledLength(LastAllPassLengths[channelNumber], sampleRate));
    }

    for (std::size_t combNumber = 0; combNumber < CombCount; ++combNumber)
    {
        float length = float(ScaledLength(CombLengths[combNumber], sampleRate));
        float exponent = PowerOfTenForSixtyDecibels * length / (float(sampleRate) * parameters.timeSeconds);
        bool hasTail = parameters.timeSeconds > 0.0f;
        combFeedbacks[combNumber] = hasTail ? static_cast<float>(std::pow(10.0, double(exponent))) : 0.0f;
    }

    // The weight of the previous sample is the one of the original at 32 000 Hz. At another rate it is raised to the power of the
    // ratio of the rates, which keeps the time the filter needs to follow a change.
    float weightAtOriginalRate = std::min(1.0f - parameters.damping, HighestDampingWeight);
    dampingWeight = static_cast<float>(std::pow(double(weightAtOriginalRate), OriginalSampleRate / double(sampleRate)));
    inputWeight = 1.0f - dampingWeight;

    double secondsUntilSilent = double(TailTimesToKeep) * parameters.timeSeconds + parameters.preDelaySeconds + SecondsOfSafety;
    samplesUntilSilent = inputDelaySamples + static_cast<std::size_t>(secondsUntilSilent * double(sampleRate));
}

const ReverbParameters& Reverb::Parameters() const
{
    return parameters;
}

// The sound of the line comes back after the length of the line, and the input is added to it.
float Reverb::Comb(DelayLine& line, float input, float feedback)
{
    float delayed = line.Oldest();
    line.Replace(input + delayed * feedback);
    return delayed;
}

// A filter that passes every tone at the same level but smears them in time.
float Reverb::AllPass(DelayLine& line, float input, float gain)
{
    float delayed = line.Oldest();
    float stored = input + delayed * gain;
    line.Replace(stored);
    return delayed - stored * gain;
}

float Reverb::ProcessChannel(Channel& channel, float input)
{
    float delayedInput = channel.preDelay.Exchange(input);

    float combSum = 0.0f;
    for (std::size_t combNumber = 0; combNumber < CombCount; ++combNumber)
        combSum += Comb(channel.combs[combNumber], delayedInput, combFeedbacks[combNumber]);

    float smeared = combSum;
    for (DelayLine& allPass : channel.allPasses)
        smeared = AllPass(allPass, smeared, parameters.coloration);

    channel.damped = inputWeight * smeared + dampingWeight * channel.damped;

    return AllPass(channel.lastAllPass, channel.damped, parameters.coloration) * TailGain;
}

void Reverb::Clear()
{
    for (Channel& channel : channels)
    {
        channel.preDelay.Clear();
        for (DelayLine& comb : channel.combs)
            comb.Clear();
        for (DelayLine& allPass : channel.allPasses)
            allPass.Clear();
        channel.lastAllPass.Clear();
        channel.damped = 0.0f;
    }
    isCleared = true;
}

void Reverb::Process(std::span<float> left, std::span<float> right)
{
    if (IsSilent(left) && IsSilent(right))
    {
        quietSamples += left.size();
        if (quietSamples > samplesUntilSilent)
        {
            if (!isCleared)
                Clear();
            return;
        }
    }
    else
    {
        quietSamples = 0;
    }

    isCleared = false;
    float crosstalkGain = CrosstalkScale * parameters.crosstalk;
    for (std::size_t sampleNumber = 0; sampleNumber < left.size(); ++sampleNumber)
    {
        float leftTail = ProcessChannel(channels[0], left[sampleNumber]);
        float rightTail = ProcessChannel(channels[1], right[sampleNumber]);
        left[sampleNumber] = (leftTail + rightTail * crosstalkGain) * parameters.mix;
        right[sampleNumber] = (rightTail + leftTail * crosstalkGain) * parameters.mix;
    }
}

}
