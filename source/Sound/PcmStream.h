#pragma once

#include "Sound/Resampler.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace Sound
{

// Sound that a program decoded itself, as 16 bit samples (the channels of one moment one after the other), played once from a place
// in it: the sound of a movie, the music of a slide show. The program's thread starts and stops it, the audio thread renders it.
class PcmStream
{
public:
    PcmStream() = default;
    PcmStream(const PcmStream&) = delete;
    PcmStream& operator=(const PcmStream&) = delete;

    // Plays the samples from startSeconds on. One or two channels; more are not played.
    void Play(std::span<const std::int16_t> interleavedSamples, int channelCount, int sampleRate, double startSeconds);
    void Stop();

    bool IsPlaying() const;
    double PositionSeconds() const;

    // Adds the next samples to the stereo buffer of the device, at its sample rate. Called by the audio thread.
    void Render(std::span<float> interleavedStereo, std::uint32_t deviceSampleRate);

private:
    mutable std::mutex mutex;
    std::vector<std::vector<float>> channels;
    int sourceSampleRate = 0;
    double position = 0;
    bool isPlaying = false;
    Resampler resampler;
};

}
