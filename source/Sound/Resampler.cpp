#include "Sound/Resampler.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Sound
{

namespace
{

// The filter reaches this many samples to the left and to the right of the position (so 2 * 24 = 48 weights are used).
// The window is flat in the passband to 0.43 of the sample rate and cuts everything above 0.57 by 100 dB.
constexpr int HalfWidth = 24;
constexpr double KaiserBeta = 10.0;

// The filter is stored with this many entries between two samples. The value between two entries is found by a straight line,
// which is wrong by less than one part in a million at this spacing.
constexpr int EntriesPerSample = 2048;

// The sum that defines the Bessel function I0 stops when the terms are smaller than this.
constexpr double BesselPrecision = 1e-20;

double BesselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    double halfX = x / 2.0;
    for (int termNumber = 1; term > BesselPrecision * sum; ++termNumber)
    {
        double factor = halfX / termNumber;
        term *= factor * factor;
        sum += term;
    }
    return sum;
}

double Sinc(double x)
{
    if (x == 0.0)
        return 1.0;

    double angle = std::numbers::pi * x;
    return std::sin(angle) / angle;
}

// The Kaiser window is 1 in the middle and falls to nearly 0 at the ends (distance +-HalfWidth).
double KaiserWindow(double distance)
{
    double position = distance / HalfWidth;
    double insideRoot = std::max(0.0, 1.0 - position * position);
    return BesselI0(KaiserBeta * std::sqrt(insideRoot)) / BesselI0(KaiserBeta);
}

}

float WaveSamples::SampleAt(std::int64_t sampleNumber) const
{
    std::int64_t sampleCount = static_cast<std::int64_t>(samples.size());
    if (sampleNumber < 0)
        return 0.0f;

    if (sampleNumber >= sampleCount)
    {
        if (!loops)
            return 0.0f;

        std::int64_t loopLength = sampleCount - loopStartSample;
        sampleNumber = loopStartSample + (sampleNumber - loopStartSample) % loopLength;
    }
    return samples[static_cast<std::size_t>(sampleNumber)];
}

Resampler::Resampler()
{
    constexpr int GuardEntryCount = 2;

    weights.resize(std::size_t(HalfWidth) * EntriesPerSample + GuardEntryCount, 0.0);
    for (std::size_t entryNumber = 0; entryNumber <= std::size_t(HalfWidth) * EntriesPerSample; ++entryNumber)
    {
        double distance = double(entryNumber) / EntriesPerSample;
        weights[entryNumber] = Sinc(distance) * KaiserWindow(distance);
    }
}

double Resampler::Weight(double distance) const
{
    double entryPosition = std::abs(distance) * EntriesPerSample;
    std::size_t entryNumber = static_cast<std::size_t>(entryPosition);
    double betweenEntries = entryPosition - double(entryNumber);

    return weights[entryNumber] + betweenEntries * (weights[entryNumber + 1] - weights[entryNumber]);
}

float Resampler::Read(const WaveSamples& wave, double position, double step) const
{
    // Reading faster than the wave's own rate stretches the filter, so that it cuts at half the rate of the device.
    double stretch = std::max(1.0, step);
    double reach = HalfWidth * stretch;

    std::int64_t firstSample = static_cast<std::int64_t>(std::ceil(position - reach));
    std::int64_t lastSample = static_cast<std::int64_t>(std::floor(position + reach));
    std::int64_t sampleCount = static_cast<std::int64_t>(wave.samples.size());
    bool allSamplesExist = firstSample >= 0 && lastSample < sampleCount;

    double sum = 0.0;
    for (std::int64_t sampleNumber = firstSample; sampleNumber <= lastSample; ++sampleNumber)
    {
        double weight = Weight((double(sampleNumber) - position) / stretch);
        double sample = allSamplesExist ? wave.samples[static_cast<std::size_t>(sampleNumber)] : wave.SampleAt(sampleNumber);
        sum += weight * sample;
    }

    // Stretching the filter makes its weights add up to more, in proportion.
    return static_cast<float>(sum / stretch);
}

}
