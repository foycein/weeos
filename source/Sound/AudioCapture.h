#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace Sound
{

class AudioCaptureError : public std::runtime_error
{
public:
    explicit AudioCaptureError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

// Thrown when Windows has no capture device (no microphone) to open.
class NoCaptureDeviceError : public AudioCaptureError
{
public:
    explicit NoCaptureDeviceError(const std::string& message)
        : AudioCaptureError(message)
    {
    }
};

// Hears the default capture device of Windows (the default microphone) through WASAPI in shared mode, like any other program, and hands
// what it hears to a function as one channel at the sample rate that was asked for (MonoStreamConverter). The device wakes the capture
// thread through an event.
//
// Nothing is kept: the samples go to the function and nowhere else. While a capture is open, Windows shows that the microphone is in use.
// A device that fails ends the capture (HasFailed); the samples stop.
//
// Create and destroy an AudioCapture on one thread.
class AudioCapture
{
public:
    // Called on the capture thread with the samples heard since the last call, from -1 to 1.
    using DeliveryFunction = std::function<void(std::span<const float> monoSamples)>;

    // Opens the default capture device and starts hearing. Throws a NoCaptureDeviceError when Windows has no capture device, and an
    // AudioCaptureError when the device does not open.
    AudioCapture(std::uint32_t outputSampleRate, DeliveryFunction delivery);

    // Stops hearing and lets the device go.
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    // The rate and the channels that Windows captures at.
    std::uint32_t DeviceSampleRate() const;
    std::uint32_t DeviceChannelCount() const;

    // The number of samples handed to the function so far.
    std::uint64_t DeliveredSampleCount() const;

    bool HasFailed() const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation;
};

}
