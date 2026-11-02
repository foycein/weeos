#pragma once

#include "Sound/WaveDecoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Channels
{

// A voice message holds up to 10 seconds of the microphone ("Record your voice (up to 10 seconds)", 10240002; the time bar of the
// comment layout runs 600 frames).
constexpr double LongestRecordingSeconds = 10.0;

// "Change Voice" raises the voice by this factor without making it shorter (Assumed: the voice changer of the original
// was not decoded).
constexpr double ChangedVoicePitch = 1.5;

// The voice of a message: the samples of the microphone, 16 bit at 16000 a second, as they were recorded.
class VoiceRecording
{
public:
    // Adds what the microphone heard, up to the longest recording. Gives the number of samples taken.
    std::size_t Add(std::span<const std::int16_t> samples);

    bool IsFull() const;
    bool IsEmpty() const;
    double Seconds() const;
    void Clear();

    const std::vector<std::int16_t>& Samples() const;

    // The recording as a wave to play, with the changed voice or as it was recorded.
    std::shared_ptr<const Sound::DecodedWave> ToWave(bool withChangedVoice) const;

private:
    std::vector<std::int16_t> samples;
};

// Raises the pitch of a voice by the factor and keeps its length: short overlapping pieces are read faster and laid one after the
// other with a smooth window, so the voice sounds higher and the message lasts as long as before.
std::vector<float> RaisePitch(std::span<const float> voice, double factor);

}
