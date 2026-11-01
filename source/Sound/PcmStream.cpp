#include "Sound/PcmStream.h"

namespace Sound
{

namespace
{

constexpr float LargestSixteenBitValue = 32768.0f;
constexpr int StereoChannels = 2;

}

void PcmStream::Play(std::span<const std::int16_t> interleavedSamples, int channelCount, int sampleRate, double startSeconds)
{
    std::vector<std::vector<float>> newChannels;
    if (channelCount >= 1 && channelCount <= StereoChannels && sampleRate > 0)
    {
        std::size_t frameCount = interleavedSamples.size() / channelCount;
        newChannels.assign(channelCount, std::vector<float>(frameCount));
        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            for (int channel = 0; channel < channelCount; ++channel)
                newChannels[channel][frame] = interleavedSamples[frame * channelCount + channel] / LargestSixteenBitValue;
        }
    }

    std::lock_guard<std::mutex> lock(mutex);
    channels = std::move(newChannels);
    sourceSampleRate = sampleRate;
    position = startSeconds * sampleRate;
    isPlaying = !channels.empty();
}

void PcmStream::Stop()
{
    std::lock_guard<std::mutex> lock(mutex);
    isPlaying = false;
}

bool PcmStream::IsPlaying() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return isPlaying;
}

double PcmStream::PositionSeconds() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (sourceSampleRate == 0)
        return 0;

    return position / sourceSampleRate;
}

void PcmStream::Render(std::span<float> interleavedStereo, std::uint32_t deviceSampleRate)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!isPlaying || deviceSampleRate == 0)
        return;

    double step = static_cast<double>(sourceSampleRate) / deviceSampleRate;
    double end = static_cast<double>(channels[0].size());
    std::size_t frameCount = interleavedStereo.size() / StereoChannels;
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        if (position >= end)
        {
            isPlaying = false;
            return;
        }

        float left = resampler.Read({channels[0], false, 0}, position, step);
        float right = channels.size() > 1 ? resampler.Read({channels[1], false, 0}, position, step) : left;
        interleavedStereo[frame * StereoChannels] += left;
        interleavedStereo[frame * StereoChannels + 1] += right;
        position += step;
    }
}

}
