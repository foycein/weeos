#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace Sound
{

class AudioOutputError : public std::runtime_error
{
public:
    explicit AudioOutputError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

// Thrown when Windows has no playback device to open.
class NoPlaybackDeviceError : public AudioOutputError
{
public:
    explicit NoPlaybackDeviceError(const std::string& message)
        : AudioOutputError(message)
    {
    }
};

// How much sound the device is given at a time. The thread runs at the normal priority, so the default is the period that Windows
// uses for its own sounds, usually 10 milliseconds. The shortest period (about 2 milliseconds) makes Windows run its whole mixer
// at that period while the output is open, which can disturb the sound of other programs.
enum class AudioLatency
{
    Normal,
    Shortest,
};

// Plays what a function makes on the default playback device of Windows, through WASAPI in shared mode like any other program.
//
// The output uses the format Windows already mixes in (32 bit floating point, the sample rate and the channels of the device),
// so Windows does not convert the sound again. The device wakes the audio thread through an event; the thread asks the function
// for the free frames and puts the left sound on the first channel and the right sound on the second.
//
// The output keeps playing when things go wrong around it. When the program is too slow, the device plays silence for a moment
// and goes on (UnderrunCount). When the user chooses another default device, or the device is unplugged, or its format changes, the
// audio thread opens the default device again, as often as it takes, until there is one (ReopenCount). It then tells the program
// the sample rate of the new device. Only a render function that throws stops the output for good (HasFailed).
//
// Create, start, stop and destroy an AudioOutput on one thread.
class AudioOutput
{
public:
    // Fills the buffer with stereo frames (left, right, left, right and so on), at the sample rate of the device. The audio thread calls it.
    // It must not wait for anything for long: no file, no lock that another thread holds for long, no memory allocation.
    // When it throws, it is not called again and the device gets silence.
    using RenderFunction = std::function<void(std::span<float> interleavedStereo)>;

    // Called on the audio thread with the sample rate of the device when the stream starts, and again each time the output has
    // opened the device again. The sample rate may be another one now; then the function that makes the sound has to follow.
    using SampleRateFunction = std::function<void(std::uint32_t sampleRate)>;

    // Opens the default playback device and prepares the stream. Nothing is played until Start.
    // Throws a NoPlaybackDeviceError when there is no device, and an AudioOutputError when it does not give the format the output needs.
    explicit AudioOutput(AudioLatency latency = AudioLatency::Normal);
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // The sample rate that Windows mixes at for the device that is open now.
    std::uint32_t SampleRate() const;

    std::uint32_t ChannelCount() const;

    // The number of frames that the device wants at a time, about.
    std::uint32_t PeriodFrames() const;

    // Mutes or unmutes only the sound of this program in the volume mixer of Windows. The volume of the device and of the other
    // programs does not change. Windows remembers the setting of a program, so a program that mutes itself must unmute when it is done.
    // The setting is made again on each device that the output opens later.
    void SetSessionMuted(bool muted);
    bool IsSessionMuted() const;

    // Starts the audio thread and the stream. The functions are called on the audio thread until Stop.
    void Start(RenderFunction renderFunction, SampleRateFunction sampleRateChanged = nullptr);

    // Stops the stream and the audio thread. Does nothing when the output is not playing.
    void Stop();

    // Makes the audio thread open the default device again, for example because the user has chosen another one in the settings.
    // It returns at once. A stopped output opens the device again when it starts.
    void ReopenDevice();

    // True when the render function threw an exception, so that the audio thread has stopped.
    bool HasFailed() const;

    // The number of frames handed to the device since the output was opened.
    std::uint64_t DeliveredFrameCount() const;

    // The number of times the device had played all it was given before the audio thread came back with more.
    std::uint64_t UnderrunCount() const;

    // The number of times the audio thread has opened the device again while it was playing.
    std::uint32_t ReopenCount() const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation;
};

}
