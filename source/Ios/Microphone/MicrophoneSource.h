#pragma once

#include <functional>
#include <span>

namespace Ios
{

// What the source hands over: mono samples from -1 to 1 at MicrophoneSampleRate, in the order they were heard.
using MicrophoneDelivery = std::function<void(std::span<const float> samples)>;

// The thing that hears: on the PC the capture device of Windows, in the tests a source that makes its own samples.
// It takes the place of the Wii Speak accessory on the USB port.
class MicrophoneSource
{
public:
    MicrophoneSource() = default;
    MicrophoneSource(const MicrophoneSource&) = delete;
    MicrophoneSource& operator=(const MicrophoneSource&) = delete;
    virtual ~MicrophoneSource() = default;

    // Starts a capture session. From now on the source calls the delivery, on any thread, until Stop. False when it cannot capture.
    virtual bool Start(MicrophoneDelivery delivery) = 0;

    // Ends the capture session. The delivery is not called any more when Stop returns.
    virtual void Stop() = 0;
};

}
