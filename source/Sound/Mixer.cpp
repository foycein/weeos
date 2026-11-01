#include "Sound/Mixer.h"

namespace Sound
{

Mixer::Mixer(std::uint32_t sampleRateOfDevice)
    : deviceSampleRate(sampleRateOfDevice)
{
}

std::uint32_t Mixer::DeviceSampleRate() const
{
    return deviceSampleRate;
}

void Mixer::SetDeviceSampleRate(std::uint32_t sampleRateOfDevice)
{
    deviceSampleRate = sampleRateOfDevice;
}

void Mixer::Mix(VoicePool& voices, const MixBuffers& buffers, std::size_t firstSampleInFrame, std::size_t frameLength)
{
    voices.Mix(resampler, double(deviceSampleRate), buffers, firstSampleInFrame, frameLength);
}

}
