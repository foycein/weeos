#include "WeeOS/ProgramSound.h"

#include <algorithm>
#include <iostream>

namespace WeeOS
{

namespace
{

std::uint64_t CountSoundingSamples(std::span<const float> samples)
{
    return std::count_if(samples.begin(), samples.end(), [](float sample) { return sample != 0.0f; });
}

}

float GainOfVolume(int volumePercent)
{
    float fraction = float(std::clamp(volumePercent, Settings::LowestVolume, Settings::HighestVolume)) / float(Settings::HighestVolume);
    return fraction * fraction;
}

ProgramSound::ProgramSound(bool silent)
    : isSilent(silent)
{
}

void ProgramSound::ApplyPcSettings(const Settings::PcSettings& settings)
{
    bool deviceHasChanged = settings.audioDevice != deviceName;
    deviceName = settings.audioDevice;
    volumeGain = GainOfVolume(settings.volume);

    if (sounds)
        sounds->SetMasterVolume(volumeGain);

    {
        std::lock_guard<std::mutex> lock(titleSoundsMutex);
        for (const std::unique_ptr<Sound::SoundSystem>& titleSound : titleSounds)
            titleSound->SetMasterVolume(volumeGain);
    }

    if (output && deviceHasChanged)
        output->ChooseDevice(deviceName);
}

Sound::SoundSystem* ProgramSound::Start(Formats::ByteView menuSoundArchive)
{
    if (hasTriedToStart)
        return sounds.get();

    hasTriedToStart = true;
    try
    {
        output = std::make_unique<Sound::AudioOutput>(Sound::AudioLatency::Normal, deviceName);
    }
    catch (const Sound::AudioOutputError& error)
    {
        std::cout << "WeeOS plays no sound: " << error.what() << "\n";
        return nullptr;
    }

    sounds = std::make_unique<Sound::SoundSystem>(Sound::ReadSoundArchiveFile(menuSoundArchive), output->SampleRate());
    sounds->SetMasterVolume(volumeGain);
    output->Start([this](std::span<float> stereo) { Render(stereo); },
        [this](std::uint32_t sampleRate) { ChangeSampleRate(sampleRate); });
    return sounds.get();
}

// The audio thread calls this when the device plays at another rate.
void ProgramSound::ChangeSampleRate(std::uint32_t sampleRate)
{
    sounds->SetDeviceSampleRate(sampleRate);

    std::lock_guard<std::mutex> lock(titleSoundsMutex);
    for (const std::unique_ptr<Sound::SoundSystem>& titleSound : titleSounds)
        titleSound->SetDeviceSampleRate(sampleRate);
}

// A silent program still runs the sound system, so that it does all the work of a normal run, and then gives the device silence.
void ProgramSound::Render(std::span<float> interleavedStereo)
{
    sounds->Render(interleavedStereo);
    AddTitleSounds(interleavedStereo);
    renderedSoundingSamples += CountSoundingSamples(interleavedStereo);

    if (isSilent)
        std::fill(interleavedStereo.begin(), interleavedStereo.end(), 0.0f);

    deliveredSoundingSamples += CountSoundingSamples(interleavedStereo);
}

void ProgramSound::AddTitleSounds(std::span<float> interleavedStereo)
{
    std::lock_guard<std::mutex> lock(titleSoundsMutex);
    titleMixBuffer.resize(interleavedStereo.size());
    for (const std::unique_ptr<Sound::SoundSystem>& titleSound : titleSounds)
    {
        titleSound->Render(titleMixBuffer);
        for (std::size_t sample = 0; sample < interleavedStereo.size(); ++sample)
            interleavedStereo[sample] += titleMixBuffer[sample];
    }
}

Sound::SoundSystem* ProgramSound::StartTitleArchive(Formats::ByteView titleSoundArchive)
{
    if (!sounds || !output)
        return nullptr;

    auto titleSound = std::make_unique<Sound::SoundSystem>(Sound::ReadSoundArchiveFile(titleSoundArchive), output->SampleRate());
    titleSound->SetMasterVolume(volumeGain);
    Sound::SoundSystem* started = titleSound.get();

    std::lock_guard<std::mutex> lock(titleSoundsMutex);
    titleSounds.push_back(std::move(titleSound));
    return started;
}

void ProgramSound::StopTitleArchives()
{
    std::lock_guard<std::mutex> lock(titleSoundsMutex);
    titleSounds.clear();
}

void ProgramSound::SetSoundMode(Settings::SoundMode mode)
{
    if (sounds)
        sounds->SetSoundMode(Sound::SoundModeFromSetting(static_cast<std::uint64_t>(mode)));

    std::lock_guard<std::mutex> lock(titleSoundsMutex);
    for (const std::unique_ptr<Sound::SoundSystem>& titleSound : titleSounds)
        titleSound->SetSoundMode(Sound::SoundModeFromSetting(static_cast<std::uint64_t>(mode)));
}

void ProgramSound::StopAllSounds()
{
    if (sounds)
        sounds->StopAll();
}

bool ProgramSound::IsSilent() const
{
    return isSilent;
}

Sound::SoundSystem* ProgramSound::Sounds()
{
    return sounds.get();
}

Sound::AudioOutput* ProgramSound::Output()
{
    return output.get();
}

std::uint64_t ProgramSound::RenderedSoundingSamples() const
{
    return renderedSoundingSamples;
}

std::uint64_t ProgramSound::DeliveredSoundingSamples() const
{
    return deliveredSoundingSamples;
}

}
