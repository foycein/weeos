#include "Ios/Microphone/MicrophoneService.h"

#include <algorithm>
#include <cmath>

namespace Ios
{

namespace
{

constexpr float LargestSampleValue = 32767.0f;
constexpr float SmallestSampleValue = -32768.0f;
constexpr float DecibelsPerAmplitudeDecade = 20.0f;

float GainOfSensitivity(int sensitivity)
{
    float decibels = SensitivityGainsInDecibels[sensitivity - LowestMicrophoneSensitivity];
    return std::pow(10.0f, decibels / DecibelsPerAmplitudeDecade);
}

}

MicrophoneService::~MicrophoneService()
{
    Close();
}

void MicrophoneService::AttachSource(MicrophoneSource& newSource)
{
    DetachSource();
    std::lock_guard<std::mutex> lock(mutex);
    if (!isClosed)
        source = &newSource;
}

void MicrophoneService::DetachSource()
{
    CloseSession();
    std::lock_guard<std::mutex> lock(mutex);
    source = nullptr;
}

void MicrophoneService::Close()
{
    DetachSource();
    std::lock_guard<std::mutex> lock(mutex);
    isClosed = true;
}

bool MicrophoneService::IsConnected()
{
    std::lock_guard<std::mutex> lock(mutex);
    return source != nullptr;
}

MicrophoneOpenResult MicrophoneService::Open()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (source == nullptr)
        return MicrophoneOpenResult::NotConnected;

    if (isOpen)
        return MicrophoneOpenResult::Opened;

    isOpen = true;
    heardSamples.clear();
    MicrophoneSource* openedSource = source;
    lock.unlock();

    // The source may deliver at once, from its own thread, so the service is unlocked while it starts.
    bool hasStarted = openedSource->Start([this](std::span<const float> samples) { Deliver(samples); });
    if (hasStarted)
        return MicrophoneOpenResult::Opened;

    lock.lock();
    isOpen = false;
    return MicrophoneOpenResult::DeviceProblem;
}

void MicrophoneService::CloseSession()
{
    std::unique_lock<std::mutex> lock(mutex);
    StopSession(lock);
}

// The source is stopped with the service unlocked: its thread may be waiting to deliver, and Stop waits for that thread.
void MicrophoneService::StopSession(std::unique_lock<std::mutex>& lock)
{
    if (!isOpen)
        return;

    isOpen = false;
    heardSamples.clear();
    MicrophoneSource* openedSource = source;
    lock.unlock();
    openedSource->Stop();
    lock.lock();
}

bool MicrophoneService::IsOpen()
{
    std::lock_guard<std::mutex> lock(mutex);
    return isOpen;
}

std::size_t MicrophoneService::TakeSamples(std::span<std::int16_t> destination)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::size_t count = std::min(destination.size(), heardSamples.size());
    std::copy_n(heardSamples.begin(), count, destination.begin());
    heardSamples.erase(heardSamples.begin(), heardSamples.begin() + static_cast<std::ptrdiff_t>(count));
    return count;
}

std::size_t MicrophoneService::AvailableSampleCount()
{
    std::lock_guard<std::mutex> lock(mutex);
    return heardSamples.size();
}

std::uint64_t MicrophoneService::DroppedSampleCount()
{
    std::lock_guard<std::mutex> lock(mutex);
    return droppedSamples;
}

void MicrophoneService::SetMuted(bool muted)
{
    std::lock_guard<std::mutex> lock(mutex);
    isMuted = muted;
}

bool MicrophoneService::IsMuted()
{
    std::lock_guard<std::mutex> lock(mutex);
    return isMuted;
}

void MicrophoneService::SetSensitivity(int newSensitivity)
{
    std::lock_guard<std::mutex> lock(mutex);
    sensitivity = std::clamp(newSensitivity, LowestMicrophoneSensitivity, HighestMicrophoneSensitivity);
    gain = GainOfSensitivity(sensitivity);
}

int MicrophoneService::Sensitivity()
{
    std::lock_guard<std::mutex> lock(mutex);
    return sensitivity;
}

void MicrophoneService::Deliver(std::span<const float> samples)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!isOpen)
        return;

    for (float sample : samples)
        heardSamples.push_back(ToStoredSample(sample));

    while (heardSamples.size() > MicrophoneBufferSamples)
    {
        heardSamples.pop_front();
        ++droppedSamples;
    }
}

// The accessory turns what it hears into 16 bit numbers after its gain, so a loud sound is cut at full scale.
std::int16_t MicrophoneService::ToStoredSample(float sample) const
{
    if (isMuted)
        return 0;

    float scaled = std::round(sample * gain * LargestSampleValue);
    float limited = std::clamp(scaled, SmallestSampleValue, LargestSampleValue);
    return static_cast<std::int16_t>(limited);
}

}
