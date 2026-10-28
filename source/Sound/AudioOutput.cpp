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
#include <format>
#include <thread>
#include <vector>

namespace Sound
{

namespace
{

using Microsoft::WRL::ComPtr;

constexpr DWORD EventWaitMilliseconds = 2000;
constexpr WORD BitsPerFloatSample = 32;

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

    if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
        return false;

    const WAVEFORMATEXTENSIBLE& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
    return IsEqualGUID(extensible.SubFormat, FloatSubFormat) != 0;
}

DeviceChannelLayout LayoutOfFormat(const WAVEFORMATEX& format)
{
    std::uint32_t channelCount = format.nChannels;
    if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
        return LayoutOfChannelMask(channelCount, FrontLeftSpeakerBit | FrontRightSpeakerBit);

    const WAVEFORMATEXTENSIBLE& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
    return LayoutOfChannelMask(channelCount, extensible.dwChannelMask);
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

}

struct AudioOutput::Implementation
{
    ~Implementation()
    {
        renderClient.Reset();
        sessionVolume.Reset();
        client.Reset();
        if (bufferReadyEvent != nullptr)
            CloseHandle(bufferReadyEvent);

        if (comIsInitialized)
            CoUninitialize();
    }

    void Open(AudioLatency latency)
    {
        // The result is a failure (and no need to undo it) when this thread has COM already, in another mode.
        comIsInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT enumeratorResult = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        ThrowIfFailed(enumeratorResult, "creating the device enumerator");

        ComPtr<IMMDevice> device;
        ThrowIfFailed(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device), "finding the default playback device");
        ThrowIfFailed(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, &client), "opening the playback device");

        WAVEFORMATEX* mixFormatPointer = nullptr;
        ThrowIfFailed(client->GetMixFormat(&mixFormatPointer), "asking for the mix format");
        MixFormat mixFormat(mixFormatPointer);
        if (!IsFloatFormat(*mixFormat) || mixFormat->wBitsPerSample != BitsPerFloatSample)
            throw AudioOutputError("Audio output: the mix format of the device is not 32 bit floating point");

        sampleRate = mixFormat->nSamplesPerSec;
        layout = LayoutOfFormat(*mixFormat);
        if (layout.leftChannel >= layout.channelCount || layout.rightChannel >= layout.channelCount)
            layout = LayoutOfChannelMask(layout.channelCount, 0);

        UINT32 defaultPeriod = 0;
        UINT32 fundamentalPeriod = 0;
        UINT32 shortestPeriod = 0;
        UINT32 longestPeriod = 0;
        HRESULT periodResult = client->GetSharedModeEnginePeriod(mixFormat.get(), &defaultPeriod, &fundamentalPeriod, &shortestPeriod, &longestPeriod);
        ThrowIfFailed(periodResult, "asking for the periods of the engine");
        periodFrames = latency == AudioLatency::Shortest ? shortestPeriod : defaultPeriod;

        ThrowIfFailed(client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, periodFrames, mixFormat.get(), nullptr),
            "starting the shared stream");
        ThrowIfFailed(client->GetBufferSize(&bufferFrameCount), "asking for the buffer size");

        bufferReadyEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (bufferReadyEvent == nullptr)
            throw AudioOutputError("Audio output: the event for the device could not be created");

        ThrowIfFailed(client->SetEventHandle(bufferReadyEvent), "giving the event to the device");
        ThrowIfFailed(client->GetService(IID_PPV_ARGS(&renderClient)), "asking for the render client");
        ThrowIfFailed(client->GetService(IID_PPV_ARGS(&sessionVolume)), "asking for the volume of the session");

        stereoFrames.resize(std::size_t(bufferFrameCount) * 2);
    }

    void Start(RenderFunction function)
    {
        Stop();

        renderFunction = std::move(function);
        stopRequested = false;
        failed = false;

        // The device wants a full buffer before it starts: silence, so that the first sound comes from the thread.
        BYTE* data = nullptr;
        ThrowIfFailed(renderClient->GetBuffer(bufferFrameCount, &data), "getting the first buffer");
        ThrowIfFailed(renderClient->ReleaseBuffer(bufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT), "giving back the first buffer");

        ThrowIfFailed(client->Start(), "starting the stream");
        audioThread = std::thread([this] { RunAudioThread(); });
    }

    void Stop()
    {
        if (!audioThread.joinable())
            return;

        stopRequested = true;
        SetEvent(bufferReadyEvent);
        audioThread.join();

        client->Stop();
        client->Reset();
    }

    void RunAudioThread()
    {
        bool threadHasCom = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

        while (!stopRequested)
        {
            DWORD waitResult = WaitForSingleObject(bufferReadyEvent, EventWaitMilliseconds);
            if (waitResult != WAIT_OBJECT_0)
                continue;

            if (!FillFreeFrames())
            {
                failed = true;
                break;
            }
        }

        if (threadHasCom)
            CoUninitialize();
    }

    // Hands all the free frames of the buffer to the device. False when the device has failed.
    bool FillFreeFrames()
    {
        UINT32 framesInBuffer = 0;
        if (FAILED(client->GetCurrentPadding(&framesInBuffer)))
            return false;

        UINT32 freeFrames = bufferFrameCount - framesInBuffer;
        if (freeFrames == 0)
            return true;

        BYTE* data = nullptr;
        if (FAILED(renderClient->GetBuffer(freeFrames, &data)))
            return false;

        RenderFrames(freeFrames, reinterpret_cast<float*>(data));
        framesDelivered += freeFrames;
        return SUCCEEDED(renderClient->ReleaseBuffer(freeFrames, 0));
    }

    void RenderFrames(UINT32 frameCount, float* deviceSamples)
    {
        std::span<float> stereo(stereoFrames.data(), std::size_t(frameCount) * 2);
        try
        {
            renderFunction(stereo);
        }
        catch (...)
        {
            // An exception must not leave the thread. The device gets silence and the output reports that it has failed.
            std::fill(stereo.begin(), stereo.end(), 0.0f);
            failed = true;
        }

        PlaceStereoOnDevice(stereo, layout, std::span<float>(deviceSamples, std::size_t(frameCount) * layout.channelCount));
    }

    ComPtr<IAudioClient3> client;
    ComPtr<IAudioRenderClient> renderClient;
    ComPtr<ISimpleAudioVolume> sessionVolume;
    HANDLE bufferReadyEvent = nullptr;
    bool comIsInitialized = false;

    std::uint32_t sampleRate = 0;
    DeviceChannelLayout layout{};
    UINT32 periodFrames = 0;
    UINT32 bufferFrameCount = 0;

    RenderFunction renderFunction;
    std::vector<float> stereoFrames;
    std::thread audioThread;
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> failed{false};
    std::atomic<std::uint64_t> framesDelivered{0};
};

AudioOutput::AudioOutput(AudioLatency latency)
    : implementation(std::make_unique<Implementation>())
{
    implementation->Open(latency);
}

AudioOutput::~AudioOutput()
{
    implementation->Stop();
}

std::uint32_t AudioOutput::SampleRate() const
{
    return implementation->sampleRate;
}

std::uint32_t AudioOutput::ChannelCount() const
{
    return implementation->layout.channelCount;
}

std::uint32_t AudioOutput::PeriodFrames() const
{
    return implementation->periodFrames;
}

void AudioOutput::SetSessionMuted(bool muted)
{
    ThrowIfFailed(implementation->sessionVolume->SetMute(muted ? TRUE : FALSE, nullptr), "muting the session");
}

bool AudioOutput::IsSessionMuted() const
{
    BOOL muted = FALSE;
    ThrowIfFailed(implementation->sessionVolume->GetMute(&muted), "asking if the session is muted");
    return muted != FALSE;
}

void AudioOutput::Start(RenderFunction renderFunction)
{
    implementation->Start(std::move(renderFunction));
}

void AudioOutput::Stop()
{
    implementation->Stop();
}

bool AudioOutput::HasFailed() const
{
    return implementation->failed;
}

std::uint64_t AudioOutput::DeliveredFrameCount() const
{
    return implementation->framesDelivered;
}

}
