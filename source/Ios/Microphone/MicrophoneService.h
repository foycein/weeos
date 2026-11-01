#pragma once

#include "Ios/Microphone/MicrophoneSource.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>

namespace Ios
{

// The Wii Speak accessory hands the channel 16 bit mono samples at 16000 a second: the rate that the Wii Speak Channel chooses from the
// three of its microphone library (8000, 11025 and 16000, 0x801B2AC8 of its program).
constexpr std::uint32_t MicrophoneSampleRate = 16000;

// The sensitivity setting of the Wii Speak Channel: five steps, 3 when nothing was saved (0x801B1C60 and 0x80091A54 of its program).
constexpr int LowestMicrophoneSensitivity = 1;
constexpr int HighestMicrophoneSensitivity = 5;
constexpr int DefaultMicrophoneSensitivity = 3;

// The gain of each sensitivity step, in decibels. The original gives the step to the signal processor inside the accessory, whose
// gains are not known (docs/audit/OpenPoints/Channels.md CHN-10, Assumed): 6 decibels a step around the middle.
constexpr std::array<float, 5> SensitivityGainsInDecibels = {-12.0f, -6.0f, 0.0f, 6.0f, 12.0f};

// The service keeps at most this much of what was heard and not taken yet; older samples are dropped, as a full buffer of the
// USB host drops them.
constexpr std::size_t MicrophoneBufferSamples = MicrophoneSampleRate;

enum class MicrophoneOpenResult
{
    Opened,

    // No accessory is on the USB port. The channel says "The Wii Speak accessory is not connected." (129796).
    NotConnected,

    // The accessory is there but does not start. The channel says "There is a problem with your Wii Speak accessory." (129794).
    DeviceProblem,
};

// The Wii Speak accessory on the USB port, in place of /dev/usb/oh0 and the microphone library above it (docs/research/Channels.md 8).
//
// The platform attaches what hears (a MicrophoneSource). A program opens the microphone, which starts a capture session of the
// source, takes the samples that were heard since it last asked, and closes it again. The accessory's own settings are here too:
// mute (its blue light goes off and it hears nothing) and the sensitivity, a gain of five steps.
//
// The source delivers on its own thread and the program takes on its thread; the service is thread safe.
class MicrophoneService
{
public:
    MicrophoneService() = default;
    ~MicrophoneService();

    MicrophoneService(const MicrophoneService&) = delete;
    MicrophoneService& operator=(const MicrophoneService&) = delete;

    // ---- The platform

    // Plugs the accessory in. The source must stay alive until it is detached or the service is closed. A source that was there is
    // detached first.
    void AttachSource(MicrophoneSource& source);

    // Takes the accessory off. An open session ends.
    void DetachSource();

    // IOS stops: the session ends and nothing can be opened any more.
    void Close();

    // ---- The programs

    bool IsConnected();

    // Starts hearing. Opening a microphone that is open already does nothing more.
    MicrophoneOpenResult Open();

    // Stops hearing and forgets what was not taken.
    void CloseSession();

    bool IsOpen();

    // Moves the oldest samples that were heard into the destination and gives their number. Muted samples are zeros.
    std::size_t TakeSamples(std::span<std::int16_t> destination);

    std::size_t AvailableSampleCount();

    // How many samples were dropped because nobody took them in time.
    std::uint64_t DroppedSampleCount();

    void SetMuted(bool muted);
    bool IsMuted();

    // LowestMicrophoneSensitivity to HighestMicrophoneSensitivity; other numbers are clamped.
    void SetSensitivity(int sensitivity);
    int Sensitivity();

private:
    void Deliver(std::span<const float> samples);
    std::int16_t ToStoredSample(float sample) const;
    void StopSession(std::unique_lock<std::mutex>& lock);

    std::mutex mutex;
    MicrophoneSource* source = nullptr;
    bool isOpen = false;
    bool isClosed = false;
    bool isMuted = false;
    int sensitivity = DefaultMicrophoneSensitivity;
    float gain = 1.0f;
    std::deque<std::int16_t> heardSamples;
    std::uint64_t droppedSamples = 0;
};

}
