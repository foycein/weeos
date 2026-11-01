#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Sound
{

// How many samples of the wave the filter reaches to the left and to the right of the position, when the wave is not read faster than
// its own rate. Read faster, the filter is stretched by the step.
constexpr int ResamplerHalfWidth = 24;

// One channel of a decoded wave, as the resampler reads it. A looping wave goes back to the loop start when it reaches
// its end, and a wave that does not loop is silent before its start and behind its end.
struct WaveSamples
{
    std::span<const float> samples;
    bool loops;
    std::uint32_t loopStartSample;

    // The sample with this number, which may lie outside the wave.
    float SampleAt(std::int64_t sampleNumber) const;
};

// Reads a wave at any position between its samples, the way a perfect digital to analog converter would.
//
// The value between two samples is the sum of the samples around it, each multiplied by a weight. The weights come
// from the sinc function, which passes frequencies below half the sample rate and nothing above. The sinc function
// goes on forever, so it is cut off with a Kaiser window. The result is a filter with a flat passband, and with
// frequencies above the passband cut down by at least 90 dB.
//
// When the wave is read faster than its own sample rate, some of its frequencies would end up above what the device can
// play and come back as wrong, lower tones (aliasing). Then the filter is stretched so that it also cuts everything the
// device cannot play.
class Resampler
{
public:
    Resampler();

    // The value of the wave at the position, which counts in samples of the wave. The step is how many samples of the wave
    // pass for each sample that is played: 1.0 plays the wave at its own rate, 0.5 plays it half as fast.
    float Read(const WaveSamples& wave, double position, double step) const;

private:
    // The weight for a sample at this distance from the position, in samples. Zero beyond the width of the filter.
    double Weight(double distance) const;

    // The filter for the distance 0 up to the half width, in small steps. The last two entries are zero, so that the
    // value between two entries can always be found.
    std::vector<double> weights;
};

}
