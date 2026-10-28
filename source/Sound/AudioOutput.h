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

// How much sound the device is given at a time. A short period lets a sound follow the menu sooner, a long period lets the
// audio thread be late without a gap in the sound. The thread runs at the normal priority, so the default is the period that
// Windows uses for its own sounds, usually 10 milliseconds. The shortest period (about 2 milliseconds) makes Windows run
// its whole mixer at that period while the output is open, which can disturb the sound of other programs.
enum class AudioLatency
{
    Normal,
    Shortest,
};

// Plays what a function makes on the default playback device of Windows, with the Windows Audio Session API (WASAPI) in shared
// mode, the way every program plays its sound while others play theirs.
//
// The format is the one Windows already mixes in (the "mix format": 32 bit floating point numbers at the sample rate and with the
// channels that the user chose for the device). The output makes exactly that format, so Windows does not convert the sound again.
// The device wakes the audio thread through an event whenever it needs more. The thread asks the function for exactly the number of
// frames that are free and hands them over, with the left sound on the first channel and the right sound on the second.
//
// Create, use and destroy an AudioOutput on one thread. Only this part of WeeOS includes the headers of WASAPI.
// When the user changes the default device or the format of the device in Windows, the output fails: HasFailed() becomes true.
// The program then creates a new AudioOutput (and a sound system for its new SampleRate).
class AudioOutput
{
public:
    // Fills the buffer with stereo frames (left, right, left, right and so on), at SampleRate(). The audio thread calls it.
    // It must not wait for anything for long: no file, no lock that another thread holds for long, no memory allocation.
    // When it throws, it is not called again and the device gets silence.
    using RenderFunction = std::function<void(std::span<float> interleavedStereo)>;

    // Opens the default playback device and prepares the stream. Nothing is played until Start.
    // Throws a NoPlaybackDeviceError when there is no device, and an AudioOutputError when it does not give the format the output needs.
    explicit AudioOutput(AudioLatency latency = AudioLatency::Normal);
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // The sample rate that Windows mixes at for the device.
    std::uint32_t SampleRate() const;

    std::uint32_t ChannelCount() const;

    // The number of frames that the device wants at a time, about.
    std::uint32_t PeriodFrames() const;

    // Mutes or unmutes only the sound of this program in the volume mixer of Windows. The volume of the device and of the other
    // programs does not change. Windows remembers the setting of a program, so a program that mutes itself must unmute when it is done.
    void SetSessionMuted(bool muted);
    bool IsSessionMuted() const;

    // Starts the audio thread and the stream. The function is called on the audio thread until Stop.
    void Start(RenderFunction renderFunction);

    // Stops the stream and the audio thread. Does nothing when the output is not playing.
    void Stop();

    // True when the audio thread has stopped because the device failed (for instance it was unplugged), or when the render function
    // threw an exception.
    bool HasFailed() const;

    // The number of frames handed to the device since the output was opened.
    std::uint64_t DeliveredFrameCount() const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation;
};

}
