#include "Sound/Voice.h"

#include <algorithm>
#include <cmath>

namespace Sound
{

namespace
{

constexpr float HighestSurroundPan = 2.0f;

// The two channels of a stereo wave are pushed to the left and to the right, before the pan of the voice is added.
constexpr float StereoChannelOffset = 1.0f;

// Minus 3 decibels. A voice behind the listener is made 3 decibels softer when it is moved to the rear speaker, and a stereo
// output plays the rear speaker with both front speakers, again 3 decibels softer.
constexpr float MinusThreeDecibels = 0.70710678f;

float PanOffsetOfChannel(std::size_t channelNumber, std::size_t channelCount)
{
    if (channelCount < 2)
        return 0.0f;

    return channelNumber == 0 ? -StereoChannelOffset : StereoChannelOffset;
}

// The two speakers share the sound so that the loudness stays the same wherever it is: at the middle both play at 0.707.
ChannelGains PanGains(float pan)
{
    float clampedPan = std::clamp(pan, -1.0f, 1.0f);

    ChannelGains gains;
    gains.left = std::sqrt((1.0f - clampedPan) / 2.0f);
    gains.right = std::sqrt((1.0f + clampedPan) / 2.0f);
    return gains;
}

}

float PanFromNumber(std::uint8_t number)
{
    constexpr int MiddleNumber = 64;
    constexpr int StepsFromMiddleToEdge = 63;
    constexpr int FirstNumberThatIsNotTooFarLeft = 2;

    int stepsFromMiddle = number - MiddleNumber;

    // The number 0 is 64 steps left of the middle, one step further than the edge. The numbers 0 and 1 are moved one step in.
    if (number < FirstNumberThatIsNotTooFarLeft)
        ++stepsFromMiddle;

    return float(stepsFromMiddle) / float(StepsFromMiddleToEdge);
}

float SurroundPanFromNumber(std::uint8_t number)
{
    constexpr float NumbersFromFrontToSide = 63.0f;

    return std::clamp(float(number) / NumbersFromFrontToSide, 0.0f, HighestSurroundPan);
}

void Voice::Start(
    std::shared_ptr<const DecodedWave> waveToPlay, const Envelope& envelopeToUse, int voicePriority, const VoiceSettings& firstSettings)
{
    wave = std::move(waveToPlay);
    envelope = envelopeToUse;
    priority = voicePriority;
    settings = firstSettings;
    active = true;
    atTheFirstFrame = true;
    endsAfterThisFrame = false;
    position = 0.0;
    gainsAtFrameStart = {};
    gainsAtFrameEnd = {};
}

bool Voice::IsActive() const
{
    return active;
}

int Voice::Priority() const
{
    return priority;
}

void Voice::Release()
{
    envelope.StartRelease();
}

void Voice::SetSettings(const VoiceSettings& newSettings)
{
    settings = newSettings;
}

void Voice::Stop()
{
    active = false;
    wave.reset();
}

std::array<ChannelGains, Voice::MaximumChannelCount> Voice::ComputeGains() const
{
    float surroundPan = std::clamp(settings.surroundPan, 0.0f, HighestSurroundPan);
    float frontGain = std::sqrt(1.0f - surroundPan / HighestSurroundPan);
    float rearGain = std::sqrt(surroundPan / HighestSurroundPan) * MinusThreeDecibels * MinusThreeDecibels;
    float loudness = settings.volume * envelope.Gain() * settings.mainSend;

    std::array<ChannelGains, MaximumChannelCount> gains{};
    for (std::size_t channelNumber = 0; channelNumber < wave->channels.size(); ++channelNumber)
    {
        float pan = settings.pan + PanOffsetOfChannel(channelNumber, wave->channels.size());
        ChannelGains panGains = PanGains(pan);
        gains[channelNumber].left = loudness * (panGains.left * frontGain + rearGain);
        gains[channelNumber].right = loudness * (panGains.right * frontGain + rearGain);
    }
    return gains;
}

void Voice::BeginFrame()
{
    if (!active)
        return;

    // The last frame was the one in which the voice faded out.
    if (endsAfterThisFrame)
    {
        Stop();
        return;
    }

    gainsAtFrameStart = gainsAtFrameEnd;
    gainsAtFrameEnd = ComputeGains();

    // A new voice starts at its gains at once. A voice that plays already moves from its gains of the last frame.
    if (atTheFirstFrame)
        gainsAtFrameStart = gainsAtFrameEnd;

    atTheFirstFrame = false;

    // When the release is over, the gains are zero. This frame fades from the gains of the frame before to zero, and then the voice ends.
    if (envelope.IsFinished())
        endsAfterThisFrame = true;

    envelope.Advance(SoundFrameMilliseconds);
}

void Voice::Mix(const Resampler& resampler, double deviceSampleRate, std::span<float> left, std::span<float> right,
    std::size_t firstSampleInFrame, std::size_t frameLength)
{
    if (!active)
        return;

    double step = double(wave->sampleRate) * settings.pitch / deviceSampleRate;
    std::size_t channelCount = wave->channels.size();
    std::size_t sampleCount = wave->SampleCount();
    std::size_t loopLength = sampleCount - wave->loopStartSample;

    std::array<WaveSamples, MaximumChannelCount> channelSamples;
    for (std::size_t channelNumber = 0; channelNumber < channelCount; ++channelNumber)
        channelSamples[channelNumber] = WaveSamples{wave->channels[channelNumber], wave->loops, wave->loopStartSample};

    for (std::size_t outputNumber = 0; outputNumber < left.size(); ++outputNumber)
    {
        float fractionOfFrame = float(firstSampleInFrame + outputNumber) / float(frameLength);

        for (std::size_t channelNumber = 0; channelNumber < channelCount; ++channelNumber)
        {
            float sample = resampler.Read(channelSamples[channelNumber], position, step);

            const ChannelGains& from = gainsAtFrameStart[channelNumber];
            const ChannelGains& to = gainsAtFrameEnd[channelNumber];
            left[outputNumber] += sample * (from.left + (to.left - from.left) * fractionOfFrame);
            right[outputNumber] += sample * (from.right + (to.right - from.right) * fractionOfFrame);
        }

        position += step;
        if (position < double(sampleCount))
            continue;

        if (!wave->loops)
        {
            Stop();
            return;
        }

        while (position >= double(sampleCount))
            position -= double(loopLength);
    }
}

}
