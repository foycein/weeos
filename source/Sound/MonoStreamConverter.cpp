#include "Sound/MonoStreamConverter.h"

#include <algorithm>
#include <cmath>

namespace Sound
{

namespace
{

// The filter needs this many samples beyond its half width: one for the sample between two entries and one to be safe.
constexpr std::size_t ExtraSamples = 2;

}

MonoStreamConverter::MonoStreamConverter(std::uint32_t inputSampleRate, std::uint32_t inputChannelCount, std::uint32_t outputSampleRate)
    : channelCount(inputChannelCount)
    , step(double(inputSampleRate) / double(outputSampleRate))
{
    double stretch = std::max(1.0, step);
    samplesAroundPosition = std::size_t(std::ceil(ResamplerHalfWidth * stretch)) + ExtraSamples;
}

std::vector<float> MonoStreamConverter::Convert(std::span<const float> interleavedFrames)
{
    std::size_t frameCount = interleavedFrames.size() / channelCount;
    for (std::size_t frameNumber = 0; frameNumber < frameCount; ++frameNumber)
    {
        std::span<const float> frame = interleavedFrames.subspan(frameNumber * channelCount, channelCount);
        float sum = 0.0f;
        for (float channelSample : frame)
            sum += channelSample;

        monoInput.push_back(sum / float(channelCount));
    }
    return MakeReadySamples();
}

std::vector<float> MonoStreamConverter::ConvertSilence(std::size_t frameCount)
{
    monoInput.insert(monoInput.end(), frameCount, 0.0f);
    return MakeReadySamples();
}

std::vector<float> MonoStreamConverter::MakeReadySamples()
{
    WaveSamples wave{monoInput, false, 0};
    std::vector<float> output;
    while (position + double(samplesAroundPosition) < double(monoInput.size()))
    {
        output.push_back(resampler.Read(wave, position, step));
        position += step;
    }
    ForgetUsedSamples();
    return output;
}

// The samples that lie more than the filter's reach before the position are not needed any more.
void MonoStreamConverter::ForgetUsedSamples()
{
    double firstNeeded = std::floor(position) - double(samplesAroundPosition);
    if (firstNeeded <= 0.0)
        return;

    std::size_t forgetCount = std::min(std::size_t(firstNeeded), monoInput.size());
    monoInput.erase(monoInput.begin(), monoInput.begin() + static_cast<std::ptrdiff_t>(forgetCount));
    position -= double(forgetCount);
}

}
