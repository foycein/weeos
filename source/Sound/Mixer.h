#pragma once

#include "Sound/Resampler.h"
#include "Sound/VoicePool.h"

#include <cstdint>
#include <span>

namespace Sound
{

// Adds the output of all voices into one stereo picture and into the input of the reverb, at the sample rate of the audio device.
// Everything is calculated in 32 bit floating point numbers. Nothing is limited or rounded here, so the sum of many loud
// voices may be louder than the device can play. That is left to the output, which is the last step.
class Mixer
{
public:
    explicit Mixer(std::uint32_t deviceSampleRate);

    std::uint32_t DeviceSampleRate() const;
    void SetDeviceSampleRate(std::uint32_t sampleRateOfDevice);

    // Adds the voices to the buffers, which the caller has cleared to zero. They are the part of
    // a sound frame that starts at firstSampleInFrame; frameLength is the length of the whole frame in samples.
    void Mix(VoicePool& voices, const MixBuffers& buffers, std::size_t firstSampleInFrame, std::size_t frameLength);

private:
    Resampler resampler;
    std::uint32_t deviceSampleRate;
};

}
