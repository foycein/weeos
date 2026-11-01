#pragma once

#include <cstdint>
#include <vector>

namespace Media
{

// Decoded sound: 16 bit samples, the channels of one moment one after the other (left, right, left, right ...).
struct PcmSound
{
    int sampleRate = 0;
    int channelCount = 0;
    std::vector<std::int16_t> samples;

    std::size_t FrameCount() const
    {
        if (channelCount == 0)
            return 0;

        return samples.size() / channelCount;
    }
};

}
