#pragma once

#include "Sound/Resampler.h"
#include "Sound/WaveDecoder.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace Sound
{

// Names a wave that was started. The handle with the number 0 names no wave.
struct WaveHandle
{
    std::uint32_t number = 0;

    bool IsValid() const
    {
        return number != 0;
    }
};

// Plays waves that a program has made or read itself, beside the sounds of the archives: the voice that the Wii Speak Channel recorded,
// a stream file such as the music of its performance check. Each wave plays once from its start to its end, at its own rate, read with
// the Resampler at the rate of the device.
//
// A mono wave plays in the middle, with the gain of the square root pan law (0.7071 on each side, as a voice of the sound system in the
// middle); a stereo wave keeps its sides.
//
// The program thread starts and stops waves; the audio thread renders. They share a short lock.
class WavePlayer
{
public:
    explicit WavePlayer(std::uint32_t deviceSampleRate);

    // Starts the wave at the volume (1 is the wave as it is). Gives no handle for a wave without samples.
    WaveHandle Play(std::shared_ptr<const DecodedWave> wave, float volume = 1.0f);

    void Stop(WaveHandle handle);
    void StopAll();

    // True from Play until the wave has played to its end or was stopped.
    bool IsPlaying(WaveHandle handle);

    // How much of the wave has been played, in seconds of the wave.
    double PlayedSeconds(WaveHandle handle);

    // The volume of PC Settings, for every wave.
    void SetMasterVolume(float volume);

    // The device plays at another rate now. Call it on the thread that renders.
    void SetDeviceSampleRate(std::uint32_t sampleRate);

    // Adds the waves that play to the buffer, left and right alternating, at the rate of the device.
    void Render(std::span<float> interleavedStereo);

private:
    struct PlayingWave
    {
        WaveHandle handle;
        std::shared_ptr<const DecodedWave> wave;
        float volume = 1.0f;
        double position = 0.0;
    };

    PlayingWave* FindPlayingWave(WaveHandle handle);
    bool RenderWave(PlayingWave& playing, std::span<float> interleavedStereo, float masterGain);

    Resampler resampler;
    std::mutex mutex;
    std::vector<PlayingWave> playingWaves;
    std::uint32_t deviceRate;
    std::uint32_t lastHandleNumber = 0;
    std::atomic<float> masterVolume{1.0f};
};

}
