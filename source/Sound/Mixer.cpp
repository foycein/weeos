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

void Mixer::Mix(VoicePool& voices, std::span<float> left, std::span<float> right, std::size_t firstSampleInFrame, std::size_t frameLength)
{
    voices.Mix(resampler, double(deviceSampleRate), left, right, firstSampleInFrame, frameLength);
}

}
