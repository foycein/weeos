#include "Media/AacMusic.h"

#include "Formats/FormatError.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <cstring>

namespace Media
{

namespace
{

using Microsoft::WRL::ComPtr;

constexpr UINT32 SixteenBits = 16;
constexpr DWORD FirstAudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
constexpr wchar_t Mp4ContentType[] = L"audio/mp4";

void Require(HRESULT result, const char* step)
{
    if (FAILED(result))
        throw Formats::FormatError(std::string("AAC music: Windows could not ") + step);
}

// Media Foundation is started for each file and stopped again; COM may already run on this thread in another mode, which is fine.
class MediaFoundationSession
{
public:
    MediaFoundationSession()
    {
        HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        hasStartedCom = SUCCEEDED(comResult);
        Require(MFStartup(MF_VERSION, MFSTARTUP_LITE), "start Media Foundation");
    }

    MediaFoundationSession(const MediaFoundationSession&) = delete;
    MediaFoundationSession& operator=(const MediaFoundationSession&) = delete;

    ~MediaFoundationSession()
    {
        MFShutdown();
        if (hasStartedCom)
            CoUninitialize();
    }

private:
    bool hasStartedCom = false;
};

ComPtr<IMFSourceReader> OpenReader(Formats::ByteView m4aFile)
{
    ComPtr<IStream> memoryStream;
    memoryStream.Attach(SHCreateMemStream(m4aFile.data(), static_cast<UINT>(m4aFile.size())));
    if (!memoryStream)
        throw Formats::FormatError("AAC music: Windows could not hold the file in memory");

    ComPtr<IMFByteStream> byteStream;
    Require(MFCreateMFByteStreamOnStream(memoryStream.Get(), &byteStream), "read the file");

    ComPtr<IMFAttributes> byteStreamAttributes;
    if (SUCCEEDED(byteStream.As(&byteStreamAttributes)))
        byteStreamAttributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, Mp4ContentType);

    ComPtr<IMFSourceReader> reader;
    Require(MFCreateSourceReaderFromByteStream(byteStream.Get(), nullptr, &reader), "open the MP4 file");
    return reader;
}

// Asks the reader for 16 bit PCM and gives back the rate and channels it decodes to.
void ChoosePcmOutput(IMFSourceReader& reader, PcmSound& sound)
{
    ComPtr<IMFMediaType> wanted;
    Require(MFCreateMediaType(&wanted), "make a media type");
    Require(wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), "ask for sound");
    Require(wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM), "ask for PCM");
    Require(wanted->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, SixteenBits), "ask for 16 bits");
    Require(reader.SetCurrentMediaType(FirstAudioStream, nullptr, wanted.Get()), "decode the AAC sound");

    ComPtr<IMFMediaType> chosen;
    Require(reader.GetCurrentMediaType(FirstAudioStream, &chosen), "tell its sound format");
    UINT32 rate = 0;
    UINT32 channels = 0;
    Require(chosen->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate), "tell the sample rate");
    Require(chosen->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels), "tell the channels");
    sound.sampleRate = static_cast<int>(rate);
    sound.channelCount = static_cast<int>(channels);
}

void AppendSample(IMFSample& sample, PcmSound& sound)
{
    ComPtr<IMFMediaBuffer> buffer;
    Require(sample.ConvertToContiguousBuffer(&buffer), "join the decoded sound");

    BYTE* data = nullptr;
    DWORD length = 0;
    Require(buffer->Lock(&data, nullptr, &length), "give the decoded sound");
    std::size_t firstNewSample = sound.samples.size();
    std::size_t newSampleCount = length / sizeof(std::int16_t);
    sound.samples.resize(firstNewSample + newSampleCount);
    std::memcpy(sound.samples.data() + firstNewSample, data, newSampleCount * sizeof(std::int16_t));
    buffer->Unlock();
}

}

PcmSound DecodeAacMusic(Formats::ByteView m4aFile)
{
    MediaFoundationSession session;
    ComPtr<IMFSourceReader> reader = OpenReader(m4aFile);

    PcmSound sound;
    ChoosePcmOutput(*reader.Get(), sound);
    while (true)
    {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        Require(reader->ReadSample(FirstAudioStream, 0, nullptr, &flags, nullptr, &sample), "decode the next piece");
        if (sample)
            AppendSample(*sample.Get(), sound);

        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
            break;
    }
    return sound;
}

}
