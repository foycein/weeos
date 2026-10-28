#include "Sound/AudioOutput.h"

#include "Sound/StereoPlacement.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <mmreg.h>

#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <thread>
#include <vector>

namespace Sound
{

namespace
{

using Microsoft::WRL::ComPtr;

// The audio thread looks at the device at least this often, even when the device does not wake it.
constexpr DWORD EventWaitMilliseconds = 2000;

// The pause between two tries to open a device when there is none.
constexpr DWORD RetryMilliseconds = 500;

constexpr WORD BitsPerFloatSample = 32;
constexpr BOOL EventResetsByItself = FALSE;
constexpr BOOL EventNeedsAReset = TRUE;
constexpr BOOL EventStartsNotSignaled = FALSE;
constexpr int MuteNotAsked = -1;

// The sub format of an extensible format that stands for floating point numbers (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT in the Windows headers).
const GUID FloatSubFormat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

void ThrowIfFailed(HRESULT result, const char* action)
{
    if (FAILED(result))
        throw AudioOutputError(std::format("Audio output: {} failed with the result {:#010x}", action, static_cast<std::uint32_t>(result)));
}

bool IsFloatFormat(const WAVEFORMATEX& format)
{
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return true;

    constexpr std::size_t ExtensionSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE || format.cbSize < ExtensionSize)
        return false;

    const WAVEFORMATEXTENSIBLE& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
    return IsEqualGUID(extensible.SubFormat, FloatSubFormat) != 0;
}

// The format that Windows hands out is freed with CoTaskMemFree.
struct MixFormatDeleter
{
    void operator()(WAVEFORMATEX* format) const
    {
        CoTaskMemFree(format);
    }
};

using MixFormat = std::unique_ptr<WAVEFORMATEX, MixFormatDeleter>;

// Hears from Windows that the user has chosen another default playback device, and wakes the audio thread.
class DeviceNotifications : public IMMNotificationClient
{
public:
    DeviceNotifications(std::atomic<bool>& flagToSet, HANDLE eventToSet)
        : defaultDeviceChanged(flagToSet)
        , wakeEvent(eventToSet)
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++referenceCount;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG remaining = --referenceCount;
        if (remaining == 0)
            delete this;

        return remaining;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void** object) override
    {
        if (!IsEqualIID(interfaceId, __uuidof(IUnknown)) && !IsEqualIID(interfaceId, __uuidof(IMMNotificationClient)))
        {
            *object = nullptr;
            return E_NOINTERFACE;
        }

        *object = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override
    {
        if (flow == eRender && role == eConsole)
        {
            defaultDeviceChanged = true;
            SetEvent(wakeEvent);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override
    {
        return S_OK;
    }

private:
    ~DeviceNotifications() = default;

    std::atomic<ULONG> referenceCount{1};
    std::atomic<bool>& defaultDeviceChanged;
    HANDLE wakeEvent;
};

// Everything that belongs to one opened device.
struct Stream
{
    ComPtr<IAudioClient3> client;
    ComPtr<IAudioRenderClient> renderClient;
    ComPtr<ISimpleAudioVolume> sessionVolume;
    std::uint32_t sampleRate = 0;
    std::uint32_t channelCount = 0;
    UINT32 periodFrames = 0;
    UINT32 bufferFrameCount = 0;
};

ComPtr<IMMDevice> FindDefaultDevice(IMMDeviceEnumerator& enumerator)
{
    ComPtr<IMMDevice> device;
    HRESULT result = enumerator.GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (result == E_NOTFOUND)
        throw NoPlaybackDeviceError("Audio output: Windows has no playback device");

    ThrowIfFailed(result, "finding the default playback device");
    return device;
}

MixFormat ReadMixFormat(IAudioClient3& client)
{
    WAVEFORMATEX* mixFormatPointer = nullptr;
    ThrowIfFailed(client.GetMixFormat(&mixFormatPointer), "asking for the mix format");

    MixFormat mixFormat(mixFormatPointer);
    if (!IsFloatFormat(*mixFormat) || mixFormat->wBitsPerSample != BitsPerFloatSample)
        throw AudioOutputError("Audio output: the mix format of the device is not 32 bit floating point");

    return mixFormat;
}

UINT32 ChoosePeriod(IAudioClient3& client, const WAVEFORMATEX& mixFormat, AudioLatency latency)
{
    UINT32 defaultPeriod = 0;
    UINT32 fundamentalPeriod = 0;
    UINT32 shortestPeriod = 0;
    UINT32 longestPeriod = 0;
    HRESULT result = client.GetSharedModeEnginePeriod(&mixFormat, &defaultPeriod, &fundamentalPeriod, &shortestPeriod, &longestPeriod);
    ThrowIfFailed(result, "asking for the periods of the engine");

    return latency == AudioLatency::Shortest ? shortestPeriod : defaultPeriod;
}

// Opens the default playback device and prepares a stream on it that wakes the thread through the event.
Stream OpenStream(IMMDeviceEnumerator& enumerator, HANDLE bufferReadyEvent, AudioLatency latency)
{
    Stream stream;
    ComPtr<IMMDevice> device = FindDefaultDevice(enumerator);
    ThrowIfFailed(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, &stream.client), "opening the playback device");

    MixFormat mixFormat = ReadMixFormat(*stream.client.Get());
    stream.sampleRate = mixFormat->nSamplesPerSec;
    stream.channelCount = mixFormat->nChannels;
    stream.periodFrames = ChoosePeriod(*stream.client.Get(), *mixFormat, latency);

    HRESULT result = stream.client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, stream.periodFrames, mixFormat.get(), nullptr);
    ThrowIfFailed(result, "starting the shared stream");
    ThrowIfFailed(stream.client->GetBufferSize(&stream.bufferFrameCount), "asking for the buffer size");
    ThrowIfFailed(stream.client->SetEventHandle(bufferReadyEvent), "giving the event to the device");
    ThrowIfFailed(stream.client->GetService(IID_PPV_ARGS(&stream.renderClient)), "asking for the render client");
    ThrowIfFailed(stream.client->GetService(IID_PPV_ARGS(&stream.sessionVolume)), "asking for the volume of the session");
    return stream;
}

}

struct AudioOutput::Implementation
{
    explicit Implementation(AudioLatency latencyToUse)
        : latency(latencyToUse)
    {
    }

    ~Implementation()
    {
        Stop();
        if (notifications)
            enumerator->UnregisterEndpointNotificationCallback(notifications.Get());

        stream = Stream{};
        if (bufferReadyEvent != nullptr)
            CloseHandle(bufferReadyEvent);
        if (wakeEvent != nullptr)
            CloseHandle(wakeEvent);

        if (comIsInitialized && std::this_thread::get_id() == creatingThread)
            CoUninitialize();
    }

    void Open()
    {
        creatingThread = std::this_thread::get_id();

        // This fails (and needs no undoing) when the thread has COM already in another mode.
        // The objects of WASAPI can be used from any thread.
        comIsInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

        bufferReadyEvent = CreateEventW(nullptr, EventResetsByItself, EventStartsNotSignaled, nullptr);
        wakeEvent = CreateEventW(nullptr, EventNeedsAReset, EventStartsNotSignaled, nullptr);
        if (bufferReadyEvent == nullptr || wakeEvent == nullptr)
            throw AudioOutputError("Audio output: the events for the device could not be created");

        ThrowIfFailed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)), "creating the device enumerator");
        notifications.Attach(new DeviceNotifications(defaultDeviceChanged, wakeEvent));
        ThrowIfFailed(enumerator->RegisterEndpointNotificationCallback(notifications.Get()), "asking Windows for news about the devices");

        InstallStream(OpenStream(*enumerator.Get(), bufferReadyEvent, latency));
    }

    // Makes the stream the one that plays. Only the thread that plays, or nobody, calls this.
    void InstallStream(Stream&& openedStream)
    {
        stereoFrames.resize(std::size_t(openedStream.bufferFrameCount) * StereoChannelCount);
        sampleRate = openedStream.sampleRate;
        channelCount = openedStream.channelCount;
        periodFrames = openedStream.periodFrames;

        std::lock_guard lock(streamMutex);
        stream = std::move(openedStream);
        if (mutedAsked != MuteNotAsked)
            stream.sessionVolume->SetMute(mutedAsked != 0 ? TRUE : FALSE, nullptr);
    }

    // Stops the device and lets go of it.
    void CloseStream()
    {
        std::lock_guard lock(streamMutex);
        if (stream.client)
        {
            stream.client->Stop();
            stream.client->Reset();
        }
        stream = Stream{};
    }

    // The device wants a full buffer before it starts: silence, so that the first sound comes from the thread.
    void StartStream()
    {
        stream.client->Reset();

        BYTE* data = nullptr;
        ThrowIfFailed(stream.renderClient->GetBuffer(stream.bufferFrameCount, &data), "getting the first buffer");
        ThrowIfFailed(stream.renderClient->ReleaseBuffer(stream.bufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT), "giving back the first buffer");
        ThrowIfFailed(stream.client->Start(), "starting the stream");
    }

    // Opens the default device again, which can be the same device as before. Gives false when that is not possible now.
    bool TryToOpenAgain()
    {
        CloseStream();
        try
        {
            InstallStream(OpenStream(*enumerator.Get(), bufferReadyEvent, latency));
            StartStream();
        }
        catch (const AudioOutputError&)
        {
            return false;
        }

        ++reopenCount;
        return true;
    }

    // Makes the stream ready to play, with a new device when the device has changed. Gives false when that is not possible now.
    bool TryToStartTheStream()
    {
        bool deviceHasChanged = defaultDeviceChanged.exchange(false);
        bool reopenWasAsked = reopenAsked.exchange(false);
        try
        {
            if (!stream.client || deviceHasChanged || reopenWasAsked)
            {
                CloseStream();
                InstallStream(OpenStream(*enumerator.Get(), bufferReadyEvent, latency));
            }
            StartStream();
        }
        catch (const AudioOutputError&)
        {
            return false;
        }
        return true;
    }

    void Start(RenderFunction function, SampleRateFunction listener)
    {
        Stop();

        renderFunction = std::move(function);
        sampleRateChanged = std::move(listener);
        stopRequested = false;
        failed = false;
        ResetEvent(wakeEvent);

        bool deviceIsOpen = TryToStartTheStream();
        StartAudioThread(deviceIsOpen);
    }

    void StartAudioThread(bool deviceIsOpen)
    {
        try
        {
            audioThread = std::thread([this, deviceIsOpen] { RunAudioThread(deviceIsOpen); });
        }
        catch (...)
        {
            std::lock_guard lock(streamMutex);
            if (stream.client)
                stream.client->Stop();
            throw;
        }
    }

    void Stop()
    {
        if (!audioThread.joinable())
            return;

        stopRequested = true;
        SetEvent(wakeEvent);
        audioThread.join();

        std::lock_guard lock(streamMutex);
        if (stream.client)
        {
            stream.client->Stop();
            stream.client->Reset();
        }
    }

    void SetSessionMuted(bool muted)
    {
        mutedAsked = muted ? 1 : 0;

        std::lock_guard lock(streamMutex);
        if (stream.sessionVolume)
            ThrowIfFailed(stream.sessionVolume->SetMute(muted ? TRUE : FALSE, nullptr), "muting the session");
    }

    bool IsSessionMuted() const
    {
        std::lock_guard lock(streamMutex);
        if (!stream.sessionVolume)
            return mutedAsked == 1;

        BOOL muted = FALSE;
        ThrowIfFailed(stream.sessionVolume->GetMute(&muted), "asking if the session is muted");
        return muted != FALSE;
    }

    void NotifySampleRate()
    {
        if (sampleRateChanged)
            sampleRateChanged(sampleRate);
    }

    // Plays until the output is stopped or the render function fails. A device that goes away, or a default device that changes,
    // makes the thread open the default device again, and it tries again every half second while there is none.
    // When the wait runs out the buffer is filled as well: that heals an event that was lost, and shows a device that has gone.
    void RunAudioThread(bool deviceIsOpenAtStart)
    {
        bool threadHasCom = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        bool deviceIsOpen = deviceIsOpenAtStart;
        if (deviceIsOpen)
            NotifySampleRate();

        while (!stopRequested && !failed)
        {
            if (reopenAsked.exchange(false) || defaultDeviceChanged.exchange(false))
                deviceIsOpen = false;

            if (!deviceIsOpen)
            {
                deviceIsOpen = TryToOpenAgain();
                if (deviceIsOpen)
                    NotifySampleRate();
                else
                    WaitForSingleObject(wakeEvent, RetryMilliseconds);

                ResetEvent(wakeEvent);
                continue;
            }

            HANDLE events[] = {bufferReadyEvent, wakeEvent};
            DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, EventWaitMilliseconds);
            if (waitResult == WAIT_OBJECT_0 + 1)
                ResetEvent(wakeEvent);
            else if (waitResult == WAIT_FAILED || !FillFreeFrames())
                deviceIsOpen = false;
        }

        if (failed && stream.client)
            stream.client->Stop();

        if (threadHasCom)
            CoUninitialize();
    }

    // Hands all the free frames of the buffer to the device. False when the device has failed.
    bool FillFreeFrames()
    {
        UINT32 framesInBuffer = 0;
        if (FAILED(stream.client->GetCurrentPadding(&framesInBuffer)))
            return false;

        UINT32 freeFrames = stream.bufferFrameCount - framesInBuffer;
        if (freeFrames == 0)
            return true;

        BYTE* data = nullptr;
        if (FAILED(stream.renderClient->GetBuffer(freeFrames, &data)))
            return false;

        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        RenderFrames(freeFrames, reinterpret_cast<float*>(data));
        CountUnderrun(framesInBuffer, std::chrono::steady_clock::now() - start);

        framesDelivered += freeFrames;
        return SUCCEEDED(stream.renderClient->ReleaseBuffer(freeFrames, 0));
    }

    // The device has played all it was given when nothing was left as the thread came back, or when making the new sound took longer
    // than the sound that was left.
    void CountUnderrun(UINT32 framesLeft, std::chrono::steady_clock::duration timeToMakeTheSound)
    {
        double secondsLeft = double(framesLeft) / double(sampleRate);
        bool ranDry = framesLeft == 0 || std::chrono::duration<double>(timeToMakeTheSound).count() > secondsLeft;
        if (ranDry)
            ++underrunCount;
    }

    void RenderFrames(UINT32 frameCount, float* deviceSamples)
    {
        std::span<float> stereo(stereoFrames.data(), std::size_t(frameCount) * StereoChannelCount);
        std::fill(stereo.begin(), stereo.end(), 0.0f);

        if (!failed)
            CallRenderFunction(stereo);

        PlaceStereoOnDevice(stereo, channelCount, std::span<float>(deviceSamples, std::size_t(frameCount) * channelCount));
    }

    // An exception must not leave the thread. The function is not called again, the device gets silence and the output reports the failure.
    void CallRenderFunction(std::span<float> stereo)
    {
        try
        {
            renderFunction(stereo);
        }
        catch (...)
        {
            std::fill(stereo.begin(), stereo.end(), 0.0f);
            failed = true;
        }
    }

    AudioLatency latency;
    bool comIsInitialized = false;
    std::thread::id creatingThread;

    HANDLE bufferReadyEvent = nullptr;
    HANDLE wakeEvent = nullptr;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<DeviceNotifications> notifications;

    // The stream is replaced only by the thread that plays, or while nobody plays. The mutex is for those who ask for the volume of the session.
    mutable std::mutex streamMutex;
    Stream stream;
    std::vector<float> stereoFrames;

    RenderFunction renderFunction;
    SampleRateFunction sampleRateChanged;
    std::thread audioThread;
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> reopenAsked{false};
    std::atomic<bool> defaultDeviceChanged{false};
    std::atomic<int> mutedAsked{MuteNotAsked};

    std::atomic<std::uint32_t> sampleRate{0};
    std::atomic<std::uint32_t> channelCount{0};
    std::atomic<std::uint32_t> periodFrames{0};
    std::atomic<std::uint64_t> framesDelivered{0};
    std::atomic<std::uint64_t> underrunCount{0};
    std::atomic<std::uint32_t> reopenCount{0};
};

AudioOutput::AudioOutput(AudioLatency latency)
    : implementation(std::make_unique<Implementation>(latency))
{
    implementation->Open();
}

AudioOutput::~AudioOutput() = default;

std::uint32_t AudioOutput::SampleRate() const
{
    return implementation->sampleRate;
}

std::uint32_t AudioOutput::ChannelCount() const
{
    return implementation->channelCount;
}

std::uint32_t AudioOutput::PeriodFrames() const
{
    return implementation->periodFrames;
}

void AudioOutput::SetSessionMuted(bool muted)
{
    implementation->SetSessionMuted(muted);
}

bool AudioOutput::IsSessionMuted() const
{
    return implementation->IsSessionMuted();
}

void AudioOutput::Start(RenderFunction renderFunction, SampleRateFunction sampleRateChanged)
{
    implementation->Start(std::move(renderFunction), std::move(sampleRateChanged));
}

void AudioOutput::Stop()
{
    implementation->Stop();
}

void AudioOutput::ReopenDevice()
{
    implementation->reopenAsked = true;
    SetEvent(implementation->wakeEvent);
}

bool AudioOutput::HasFailed() const
{
    return implementation->failed;
}

std::uint64_t AudioOutput::DeliveredFrameCount() const
{
    return implementation->framesDelivered;
}

std::uint64_t AudioOutput::UnderrunCount() const
{
    return implementation->underrunCount;
}

std::uint32_t AudioOutput::ReopenCount() const
{
    return implementation->reopenCount;
}

}
