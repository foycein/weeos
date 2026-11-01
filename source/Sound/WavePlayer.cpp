#include "Sound/WavePlayer.h"

#include <algorithm>

namespace Sound
{

namespace
{

constexpr std::size_t StereoChannelCount = 2;
constexpr std::size_t LeftChannel = 0;
constexpr std::size_t RightChannel = 1;

// The gain on each side of a sound in the middle, by the square root pan law of the sound system.
constexpr float MiddlePanGain = 0.70710678f;

}

WavePlayer::WavePlayer(std::uint32_t deviceSampleRate)
    : deviceRate(deviceSampleRate)
{
}

WaveHandle WavePlayer::Play(std::shared_ptr<const DecodedWave> wave, float volume)
{
    if (!wave || wave->SampleCount() == 0)
        return WaveHandle{};

    std::lock_guard<std::mutex> lock(mutex);
    ++lastHandleNumber;
    WaveHandle handle{lastHandleNumber};
    playingWaves.push_back(PlayingWave{handle, std::move(wave), volume, 0.0});
    return handle;
}

void WavePlayer::Stop(WaveHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::erase_if(playingWaves, [handle](const PlayingWave& playing) { return playing.handle.number == handle.number; });
}

void WavePlayer::StopAll()
{
    std::lock_guard<std::mutex> lock(mutex);
    playingWaves.clear();
}

bool WavePlayer::IsPlaying(WaveHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    return FindPlayingWave(handle) != nullptr;
}

std::size_t WavePlayer::PlayingWaveCount()
{
    std::lock_guard<std::mutex> lock(mutex);
    return playingWaves.size();
}

double WavePlayer::PlayedSeconds(WaveHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    PlayingWave* playing = FindPlayingWave(handle);
    if (playing == nullptr)
        return 0.0;

    return playing->position / double(playing->wave->sampleRate);
}

void WavePlayer::SetMasterVolume(float volume)
{
    masterVolume = volume;
}

void WavePlayer::SetDeviceSampleRate(std::uint32_t sampleRate)
{
    std::lock_guard<std::mutex> lock(mutex);
    deviceRate = sampleRate;
}

void WavePlayer::Render(std::span<float> interleavedStereo)
{
    float masterGain = masterVolume;
    std::lock_guard<std::mutex> lock(mutex);
    std::erase_if(playingWaves, [&](PlayingWave& playing) { return !RenderWave(playing, interleavedStereo, masterGain); });
}

WavePlayer::PlayingWave* WavePlayer::FindPlayingWave(WaveHandle handle)
{
    for (PlayingWave& playing : playingWaves)
    {
        if (playing.handle.number == handle.number)
            return &playing;
    }
    return nullptr;
}

// Adds one wave to the buffer. False when the wave has reached its end.
bool WavePlayer::RenderWave(PlayingWave& playing, std::span<float> interleavedStereo, float masterGain)
{
    const DecodedWave& wave = *playing.wave;
    double step = double(wave.sampleRate) / double(deviceRate);
    double endPosition = double(wave.SampleCount());
    bool isMono = wave.channels.size() == 1;
    float gain = playing.volume * masterGain * (isMono ? MiddlePanGain : 1.0f);

    WaveSamples left{wave.channels[LeftChannel], false, 0};
    WaveSamples right{wave.channels[isMono ? LeftChannel : RightChannel], false, 0};

    std::size_t frameCount = interleavedStereo.size() / StereoChannelCount;
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        if (playing.position >= endPosition)
            return false;

        interleavedStereo[frame * StereoChannelCount + LeftChannel] += gain * resampler.Read(left, playing.position, step);
        interleavedStereo[frame * StereoChannelCount + RightChannel] += gain * resampler.Read(right, playing.position, step);
        playing.position += step;
    }
    return playing.position < endPosition;
}

}
