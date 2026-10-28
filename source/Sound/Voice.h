#pragma once

#include "Sound/Envelope.h"
#include "Sound/Resampler.h"
#include "Sound/WaveDecoder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Sound
{

// The length of one sound frame in milliseconds. Sequences, envelopes and changes of volume and pan move on by one frame at a time.
constexpr int SoundFrameMilliseconds = 3;

// What an owner of a voice (a sound or a sequence track) decides for the voice. The voice takes it over at the next sound frame.
struct VoiceSettings
{
    // How loud the voice is, from 0 to 1, before its envelope.
    float volume = 1.0f;

    // -1 is the left, 0 the middle and 1 the right.
    float pan = 0.0f;

    // 0 is in front. 2 is behind. A stereo output plays what is behind with the front speakers.
    float surroundPan = 0.0f;

    // How much of the voice goes to the speakers, from 0 to 1.
    float mainSend = 1.0f;

    // How much faster the wave is played than at its own rate. 2 is an octave up.
    float pitch = 1.0f;
};

// The files give a pan as a number from 0 (left) over 64 (middle) to 127 (right). The voice wants -1 to 1.
float PanFromNumber(std::uint8_t number);

// The files give a surround pan from 0 (front) to 127 (behind). The voice wants 0 to 2.
float SurroundPanFromNumber(std::uint8_t number);

// The gains of one channel of the wave to the left and to the right output.
struct ChannelGains
{
    float left = 0.0f;
    float right = 0.0f;
};

// A voice plays one wave. It reads the wave at the pitch of the voice and at the rate of the device, and adds it to the
// outputs with its volume, pan and envelope.
class Voice
{
public:
    // The most channels a wave may have.
    static constexpr std::size_t MaximumChannelCount = 2;

    void Start(
        std::shared_ptr<const DecodedWave> waveToPlay, const Envelope& envelopeToUse, int voicePriority, const VoiceSettings& firstSettings);

    bool IsActive() const;
    int Priority() const;

    // Starts the release of the envelope. The voice ends when it is silent.
    void Release();

    void SetSettings(const VoiceSettings& newSettings);

    // Stops the voice at once.
    void Stop();

    // Decides the gains for the coming sound frame, from the settings and the envelope, and moves the envelope on.
    // The gains of the last frame are the gains at the start of this one, so that the change is smooth.
    void BeginFrame();

    // Adds count samples of the voice to the outputs, which start at the sample in the frame given.
    // frameLength is the number of samples of the whole frame.
    void Mix(const Resampler& resampler, double deviceSampleRate, std::span<float> left, std::span<float> right,
        std::size_t firstSampleInFrame, std::size_t frameLength);

private:
    std::array<ChannelGains, MaximumChannelCount> ComputeGains() const;

    std::shared_ptr<const DecodedWave> wave;
    Envelope envelope{0, 0, 0, 0};
    VoiceSettings settings;
    int priority = 0;
    bool active = false;
    bool atTheFirstFrame = true;
    bool endsAfterThisFrame = false;
    double position = 0.0;
    std::array<ChannelGains, MaximumChannelCount> gainsAtFrameStart{};
    std::array<ChannelGains, MaximumChannelCount> gainsAtFrameEnd{};
};

}
