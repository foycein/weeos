#include "Sound/StereoPlacement.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace Sound
{

namespace
{

constexpr float LoudestSample = 1.0f;

float ForTheDevice(float sample)
{
    if (std::isnan(sample))
        return 0.0f;

    return std::clamp(sample, -LoudestSample, LoudestSample);
}

// The speakers that come before this speaker in the mask are the channels that come before it in a frame.
std::uint32_t ChannelOfSpeaker(std::uint32_t channelMask, std::uint32_t speakerBit)
{
    return static_cast<std::uint32_t>(std::popcount(channelMask & (speakerBit - 1)));
}

}

void PlaceStereoOnDevice(std::span<const float> stereoFrames, const DeviceChannelLayout& layout, std::span<float> deviceFrames)
{
    std::size_t frameCount = stereoFrames.size() / 2;

    std::fill(deviceFrames.begin(), deviceFrames.end(), 0.0f);
    for (std::size_t frameNumber = 0; frameNumber < frameCount; ++frameNumber)
    {
        float left = stereoFrames[2 * frameNumber];
        float right = stereoFrames[2 * frameNumber + 1];
        float* deviceFrame = &deviceFrames[frameNumber * layout.channelCount];

        if (layout.channelCount == 1)
        {
            deviceFrame[0] = ForTheDevice((left + right) / 2.0f);
            continue;
        }

        deviceFrame[layout.leftChannel] = ForTheDevice(left);
        deviceFrame[layout.rightChannel] = ForTheDevice(right);
    }
}

DeviceChannelLayout LayoutOfChannelMask(std::uint32_t channelCount, std::uint32_t channelMask)
{
    bool hasBothFrontSpeakers = (channelMask & FrontLeftSpeakerBit) != 0 && (channelMask & FrontRightSpeakerBit) != 0;
    if (!hasBothFrontSpeakers)
        return {channelCount, 0, channelCount > 1 ? 1u : 0u};

    return {channelCount, ChannelOfSpeaker(channelMask, FrontLeftSpeakerBit), ChannelOfSpeaker(channelMask, FrontRightSpeakerBit)};
}

}
