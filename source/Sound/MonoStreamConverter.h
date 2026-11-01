#pragma once

#include "Sound/Resampler.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Sound
{

// Turns the frames of a capture device (several channels at the rate of the device) into one channel at another rate, piece by piece
// as the device delivers them. The channels are averaged, and the windowed sinc filter of the Resampler changes the rate; going down
// it is stretched, so that nothing above half of the new rate comes back as a wrong tone.
//
// A sample can only be made when the filter sees enough of the device's samples after it, so the output lags the input by the half
// width of the filter (about 1.5 milliseconds going from 48000 to 16000 a second).
class MonoStreamConverter
{
public:
    MonoStreamConverter(std::uint32_t inputSampleRate, std::uint32_t inputChannelCount, std::uint32_t outputSampleRate);

    // Adds frames of the device (the channels of a frame one after the other) and gives the samples that can be made now.
    std::vector<float> Convert(std::span<const float> interleavedFrames);

    // The same for silence of this many frames, which a device reports instead of samples.
    std::vector<float> ConvertSilence(std::size_t frameCount);

private:
    std::vector<float> MakeReadySamples();
    void ForgetUsedSamples();

    Resampler resampler;
    std::uint32_t channelCount;
    double step;
    std::size_t samplesAroundPosition;

    // The mono samples of the device that the filter may still need, and where the next output sample lies among them.
    std::vector<float> monoInput;
    double position = 0.0;
};

}
