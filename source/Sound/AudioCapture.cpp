#include "Sound/AudioCapture.h"

#include "Sound/MonoStreamConverter.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <mmreg.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <atomic>
#include <format>
#include <iterator>
#include <optional>
#include <thread>
#include <vector>

namespace Sound
{

namespace
{

using Microsoft::WRL::ComPtr;

// The capture thread looks at the device at least this often, even when the device does not wake it.
constexpr DWORD EventWaitMilliseconds = 2000;

// How much the device may hold before the thread takes it, in units of 100 nanoseconds: 200 milliseconds.
constexpr REFERENCE_TIME DeviceBufferDuration = 2'000'000;

constexpr WORD BitsPerFloatSample = 32;
constexpr BOOL EventResetsByItself = FALSE;
constexpr BOOL EventStartsNotSignaled = FALSE;
constexpr BOOL WaitForAnyEvent = FALSE;

// The sub format of an extensible format that stands for floating point numbers (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT in the Windows headers).
const GUID FloatSubFormat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

void ThrowIfFailed(HRESULT result, const char* action)
{
    if (FAILED(result))
        throw AudioCaptureError(std::format("Audio capture: {} failed with the result {:#010x}", action, static_cast<std::uint32_t>(result)));
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

ComPtr<IMMDevice> FindDefaultCaptureDevice()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    ThrowIfFailed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)), "creating the device enumerator");

    ComPtr<IMMDevice> device;
    HRESULT result = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
    if (result == E_NOTFOUND)
        throw NoCaptureDeviceError("Audio capture: Windows has no capture device");

    ThrowIfFailed(result, "finding the default capture device");
    return device;
}

MixFormat ReadMixFormat(IAudioClient& client)
{
    WAVEFORMATEX* mixFormatPointer = nullptr;
    ThrowIfFailed(client.GetMixFormat(&mixFormatPointer), "asking for the mix format");

    MixFormat mixFormat(mixFormatPointer);
    if (!IsFloatFormat(*mixFormat) || mixFormat->wBitsPerSample != BitsPerFloatSample)
        throw AudioCaptureError("Audio capture: the mix format of the device is not 32 bit floating point");

    return mixFormat;
}

}

struct AudioCapture::Implementation
{
    Implementation(std::uint32_t outputRate, DeliveryFunction function)
        : outputSampleRate(outputRate)
        , delivery(std::move(function))
    {
    }

    ~Implementation()
    {
        StopThread();
        if (client)
            client->Stop();

        captureClient.Reset();
        client.Reset();
        if (bufferReadyEvent != nullptr)
            CloseHandle(bufferReadyEvent);
        if (stopEvent != nullptr)
            CloseHandle(stopEvent);

        if (comIsInitialized)
            CoUninitialize();
    }

    void Open()
    {
        // This fails (and needs no undoing) when the thread has COM already in another mode. The objects of WASAPI can be used from any thread.
        comIsInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

        bufferReadyEvent = CreateEventW(nullptr, EventResetsByItself, EventStartsNotSignaled, nullptr);
        stopEvent = CreateEventW(nullptr, EventResetsByItself, EventStartsNotSignaled, nullptr);
        if (bufferReadyEvent == nullptr || stopEvent == nullptr)
            throw AudioCaptureError("Audio capture: the events for the device could not be created");

        ComPtr<IMMDevice> device = FindDefaultCaptureDevice();
        ThrowIfFailed(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client), "opening the capture device");

        MixFormat mixFormat = ReadMixFormat(*client.Get());
        deviceSampleRate = mixFormat->nSamplesPerSec;
        deviceChannelCount = mixFormat->nChannels;
        converter.emplace(deviceSampleRate, deviceChannelCount, outputSampleRate);

        HRESULT result = client->Initialize(
            AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, DeviceBufferDuration, 0, mixFormat.get(), nullptr);
        ThrowIfFailed(result, "starting the shared stream");
        ThrowIfFailed(client->SetEventHandle(bufferReadyEvent), "giving the event to the device");
        ThrowIfFailed(client->GetService(IID_PPV_ARGS(&captureClient)), "asking for the capture client");
        ThrowIfFailed(client->Start(), "starting the capture");

        captureThread = std::thread([this] { RunCaptureThread(); });
    }

    void StopThread()
    {
        if (!captureThread.joinable())
            return;

        SetEvent(stopEvent);
        captureThread.join();
    }

    void RunCaptureThread()
    {
        bool threadHasCom = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        HANDLE events[] = {bufferReadyEvent, stopEvent};
        constexpr DWORD StopEventResult = WAIT_OBJECT_0 + 1;
        while (true)
        {
            DWORD waitResult = WaitForMultipleObjects(static_cast<DWORD>(std::size(events)), events, WaitForAnyEvent, EventWaitMilliseconds);
            if (waitResult == StopEventResult || waitResult == WAIT_FAILED)
                break;

            if (!TakeEverythingHeard())
            {
                failed = true;
                break;
            }
        }

        if (threadHasCom)
            CoUninitialize();
    }

    // Takes every packet that the device holds and hands it on. False when the device fails.
    bool TakeEverythingHeard()
    {
        UINT32 packetFrames = 0;
        if (FAILED(captureClient->GetNextPacketSize(&packetFrames)))
            return false;

        while (packetFrames > 0)
        {
            if (!TakePacket())
                return false;

            if (FAILED(captureClient->GetNextPacketSize(&packetFrames)))
                return false;
        }
        return true;
    }

    bool TakePacket()
    {
        BYTE* data = nullptr;
        UINT32 frameCount = 0;
        DWORD flags = 0;
        if (FAILED(captureClient->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr)))
            return false;

        std::vector<float> samples;
        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0)
        {
            samples = converter->ConvertSilence(frameCount);
        }
        else
        {
            std::span<const float> frames(reinterpret_cast<const float*>(data), std::size_t(frameCount) * deviceChannelCount);
            samples = converter->Convert(frames);
        }

        if (FAILED(captureClient->ReleaseBuffer(frameCount)))
            return false;

        deliveredSamples += samples.size();
        delivery(samples);
        return true;
    }

    std::uint32_t outputSampleRate;
    DeliveryFunction delivery;
    bool comIsInitialized = false;
    HANDLE bufferReadyEvent = nullptr;
    HANDLE stopEvent = nullptr;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> captureClient;
    std::uint32_t deviceSampleRate = 0;
    std::uint32_t deviceChannelCount = 0;
    std::optional<MonoStreamConverter> converter;
    std::thread captureThread;
    std::atomic<std::uint64_t> deliveredSamples{0};
    std::atomic<bool> failed{false};
};

AudioCapture::AudioCapture(std::uint32_t outputSampleRate, DeliveryFunction delivery)
    : implementation(std::make_unique<Implementation>(outputSampleRate, std::move(delivery)))
{
    implementation->Open();
}

AudioCapture::~AudioCapture() = default;

std::uint32_t AudioCapture::DeviceSampleRate() const
{
    return implementation->deviceSampleRate;
}

std::uint32_t AudioCapture::DeviceChannelCount() const
{
    return implementation->deviceChannelCount;
}

std::uint64_t AudioCapture::DeliveredSampleCount() const
{
    return implementation->deliveredSamples;
}

bool AudioCapture::HasFailed() const
{
    return implementation->failed;
}

}
