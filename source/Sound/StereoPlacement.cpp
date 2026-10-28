#include "Sound/StereoPlacement.h"

#include <algorithm>
#include <cmath>

namespace Sound
{

namespace
{

constexpr float LoudestSample = 1.0f;

float LimitToTheRangeOfTheDevice(float sample)
{
    if (std::isnan(sample))
        return 0.0f;

    return std::clamp(sample, -LoudestSample, LoudestSample);
}

}

void PlaceStereoOnDevice(std::span<const float> stereoFrames, std::size_t deviceChannelCount, std::span<float> deviceFrames)
{
    std::size_t frameCount = stereoFrames.size() / StereoChannelCount;

    std::fill(deviceFrames.begin(), deviceFrames.end(), 0.0f);
    for (std::size_t frameNumber = 0; frameNumber < frameCount; ++frameNumber)
    {
        float left = stereoFrames[StereoChannelCount * frameNumber];
        float right = stereoFrames[StereoChannelCount * frameNumber + 1];
        float* deviceFrame = &deviceFrames[frameNumber * deviceChannelCount];

        if (deviceChannelCount == 1)
        {
            deviceFrame[0] = LimitToTheRangeOfTheDevice((left + right) / 2.0f);
            continue;
        }

        deviceFrame[0] = LimitToTheRangeOfTheDevice(left);
        deviceFrame[1] = LimitToTheRangeOfTheDevice(right);
    }
}

}
