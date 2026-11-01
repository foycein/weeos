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

// Minus 3 decibels: the loudness table of the original at -3.0 (0x8150f31c). In the sound mode Surround the rear channel of a
// voice is made this much softer.
constexpr float RearChannelGain = 0.70794578f;

// Assumed: a stereo output plays the rear channel on both speakers, each at minus 3 decibels. The AX library does this in
// its DSP program, which is not in the menu's program.
constexpr float RearToSpeakersGain = 0.70710678f;

// How one channel of a wave is spread over the outputs: the gains of the front speakers, and the gain of the rear that both speakers get.
struct Placement
{
    ChannelGains front;
    float rear = 0.0f;
};

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

// Mono puts everything in the middle. Stereo uses the pan. Surround also moves the sound to the rear with the surround pan,
// and the front gets softer as the rear gets louder.
Placement PlaceChannel(SoundMode mode, float pan, float surroundPan)
{
    Placement placement;
    if (mode == SoundMode::Mono)
    {
        placement.front = PanGains(0.0f);
        return placement;
    }

    placement.front = PanGains(pan);
    if (mode == SoundMode::Stereo)
        return placement;

    float clampedSurroundPan = std::clamp(surroundPan, 0.0f, HighestSurroundPan);
    float frontGain = std::sqrt(1.0f - clampedSurroundPan / HighestSurroundPan);
    placement.front.left *= frontGain;
    placement.front.right *= frontGain;
    placement.rear = std::sqrt(clampedSurroundPan / HighestSurroundPan) * RearChannelGain * RearToSpeakersGain;
    return placement;
}

ChannelGains Spread(const Placement& placement, float loudness)
{
    ChannelGains gains;
    gains.left = loudness * (placement.front.left + placement.rear);
    gains.right = loudness * (placement.front.right + placement.rear);
    return gains;
}

float Interpolate(float from, float to, float fraction)
{
    return from + (to - from) * fraction;
}

using GainsOfChannels = std::array<MixGains, Voice::MaximumChannelCount>;

bool SendsToReverb(const GainsOfChannels& atStart, const GainsOfChannels& atEnd)
{
    for (std::size_t channelNumber = 0; channelNumber < Voice::MaximumChannelCount; ++channelNumber)
    {
        const ChannelGains& start = atStart[channelNumber].reverb;
        const ChannelGains& end = atEnd[channelNumber].reverb;
        if (start.left != 0.0f || start.right != 0.0f || end.left != 0.0f || end.right != 0.0f)
            return true;
    }
    return false;
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
    isHeld = false;
    atTheFirstFrame = true;
    endsAfterThisFrame = false;
    position = 0.0;
    gainsAtFrameStart = {};
    gainsAtFrameEnd = {};
    lowPassFilters = {};
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

void Voice::ChangeEnvelopeNumbers(std::uint8_t attack, std::uint8_t decay, std::uint8_t sustain, std::uint8_t release)
{
    if (attack != EnvelopeNumberNotSet)
        envelope.SetAttack(attack);

    if (decay != EnvelopeNumberNotSet)
        envelope.SetDecay(decay);

    if (sustain != EnvelopeNumberNotSet)
        envelope.SetSustain(sustain);

    if (release != EnvelopeNumberNotSet)
        envelope.SetRelease(release);
}

void Voice::Stop()
{
    active = false;
    wave.reset();
}

void Voice::SetHeld(bool held)
{
    isHeld = held;
}

std::array<MixGains, Voice::MaximumChannelCount> Voice::ComputeGains(SoundMode mode) const
{
    float loudness = settings.volume * envelope.Gain();

    std::array<MixGains, MaximumChannelCount> gains{};
    for (std::size_t channelNumber = 0; channelNumber < wave->channels.size(); ++channelNumber)
    {
        float pan = settings.pan + PanOffsetOfChannel(channelNumber, wave->channels.size());
        Placement placement = PlaceChannel(mode, pan, settings.surroundPan);
        gains[channelNumber].speakers = Spread(placement, loudness * settings.mainSend);
        gains[channelNumber].reverb = Spread(placement, loudness * settings.effectSend);
    }
    return gains;
}

void Voice::BeginFrame(SoundMode mode)
{
    if (!active)
        return;

    // The last frame was the one in which the voice faded out.
    if (endsAfterThisFrame)
    {
        Stop();
        return;
    }

    // A held voice is silent. When it goes on, its first frame rises from silence.
    if (isHeld)
    {
        gainsAtFrameStart = {};
        gainsAtFrameEnd = {};
        return;
    }

    gainsAtFrameStart = gainsAtFrameEnd;
    gainsAtFrameEnd = ComputeGains(mode);

    // A new voice starts at its gains at once. A voice that plays already moves from its gains of the last frame.
    if (atTheFirstFrame)
        gainsAtFrameStart = gainsAtFrameEnd;

    atTheFirstFrame = false;

    // When the release is over, the gains are zero. This frame fades from the gains of the frame before to zero, and then the voice ends.
    if (envelope.IsFinished())
        endsAfterThisFrame = true;

    envelope.Advance(SoundFrameMilliseconds);
}

namespace
{

// Adds one sample of the wave to the speakers, and to the reverb when the voice sends to it, with the gains of this point of the frame.
void AddSample(const MixBuffers& buffers, std::size_t outputNumber, float sample, const MixGains& from, const MixGains& to,
    float fractionOfFrame, bool sendsToReverb)
{
    buffers.left[outputNumber] += sample * Interpolate(from.speakers.left, to.speakers.left, fractionOfFrame);
    buffers.right[outputNumber] += sample * Interpolate(from.speakers.right, to.speakers.right, fractionOfFrame);
    if (!sendsToReverb)
        return;

    buffers.reverbLeft[outputNumber] += sample * Interpolate(from.reverb.left, to.reverb.left, fractionOfFrame);
    buffers.reverbRight[outputNumber] += sample * Interpolate(from.reverb.right, to.reverb.right, fractionOfFrame);
}

}

void Voice::Mix(const Resampler& resampler, double deviceSampleRate, const MixBuffers& buffers, std::size_t firstSampleInFrame,
    std::size_t frameLength)
{
    if (!active || isHeld)
        return;

    double step = double(wave->sampleRate) * settings.pitch / deviceSampleRate;
    std::size_t channelCount = wave->channels.size();
    std::size_t sampleCount = wave->SampleCount();
    std::size_t loopLength = sampleCount - wave->loopStartSample;
    bool sendsToReverb = SendsToReverb(gainsAtFrameStart, gainsAtFrameEnd);

    std::array<WaveSamples, MaximumChannelCount> channelSamples;
    for (std::size_t channelNumber = 0; channelNumber < channelCount; ++channelNumber)
    {
        channelSamples[channelNumber] = WaveSamples{wave->channels[channelNumber], wave->loops, wave->loopStartSample};
        lowPassFilters[channelNumber].SetValue(settings.lowPass, deviceSampleRate);
    }

    for (std::size_t outputNumber = 0; outputNumber < buffers.left.size(); ++outputNumber)
    {
        float fractionOfFrame = float(firstSampleInFrame + outputNumber) / float(frameLength);

        for (std::size_t channelNumber = 0; channelNumber < channelCount; ++channelNumber)
        {
            float sample = resampler.Read(channelSamples[channelNumber], position, step);
            sample = lowPassFilters[channelNumber].Process(sample);
            AddSample(buffers, outputNumber, sample, gainsAtFrameStart[channelNumber], gainsAtFrameEnd[channelNumber], fractionOfFrame,
                sendsToReverb);
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
