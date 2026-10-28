#include "Sound/SoundSystem.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <format>

namespace Sound
{

namespace
{

constexpr float HighestNumber = 127.0f;
constexpr std::uint64_t MillisecondsPerSecond = 1000;

// The most samples that one sound frame can have at a sample rate: its length rounded up.
std::size_t LongestFrameLength(std::uint32_t sampleRate)
{
    return std::size_t(sampleRate) * SoundFrameMilliseconds / MillisecondsPerSecond + 1;
}

// The number of samples that have been played when a sound frame starts. At 44100 Hz a frame has 132.3 samples,
// so some frames have 132 and some 133 and the frames do not drift away from the time.
std::uint64_t SamplesBeforeFrame(std::uint64_t frameNumber, std::uint32_t sampleRate)
{
    return frameNumber * sampleRate * SoundFrameMilliseconds / MillisecondsPerSecond;
}

}

SoundSystem::SoundSystem(SoundArchiveFile archiveToPlay, std::uint32_t deviceSampleRate)
    : archive(std::move(archiveToPlay))
    , mixer(deviceSampleRate)
{
    for (const SoundInfo& sound : archive.sounds)
    {
        if (sound.kind != SoundKind::WaveSound || waveSoundFiles.contains(sound.fileId))
            continue;

        LoadedWaveSoundFile loaded = LoadWaveSoundFile(archive, sound.fileId);
        if (sound.waveSound.soundNumber >= loaded.file.sounds.size())
        {
            throw Formats::FormatError(std::format("Sound system: the sound {} is the sound {} of the file {}, which has {} sounds", sound.name,
                sound.waveSound.soundNumber, sound.fileId, loaded.file.sounds.size()));
        }
        waveSoundFiles.emplace(sound.fileId, std::move(loaded));
    }

    leftSamples.resize(LongestFrameLength(deviceSampleRate));
    rightSamples.resize(LongestFrameLength(deviceSampleRate));
}

const SoundArchiveFile& SoundSystem::Archive() const
{
    return archive;
}

SoundHandle SoundSystem::Play(std::size_t soundNumber)
{
    if (soundNumber >= archive.sounds.size())
        return SoundHandle{};

    SoundHandle handle{++lastHandleNumber};
    if (!commands.Push({SoundCommandKind::Play, handle.number, soundNumber}))
        return SoundHandle{};

    return handle;
}

void SoundSystem::Stop(SoundHandle handle)
{
    commands.Push({SoundCommandKind::Stop, handle.number, 0});
}

void SoundSystem::StopAll()
{
    commands.Push({SoundCommandKind::StopAll, 0, 0});
}

void SoundSystem::SetMasterVolume(float volume)
{
    masterVolume.store(volume);
}

void SoundSystem::SetPaused(bool shouldBePaused)
{
    paused.store(shouldBePaused);
}

std::size_t SoundSystem::ActiveVoiceCount() const
{
    return voices.ActiveVoiceCount();
}

std::size_t SoundSystem::RunningSoundCount() const
{
    return runningSounds.size();
}

void SoundSystem::ProcessCommands()
{
    SoundCommand command;
    while (commands.Pop(command))
    {
        if (command.kind == SoundCommandKind::Play)
        {
            StartSound(command);
            continue;
        }

        for (RunningSound& sound : runningSounds)
        {
            if (command.kind == SoundCommandKind::StopAll || sound.handle.number == command.handleNumber)
                StopRunningSound(sound);
        }
    }
}

void SoundSystem::StartSound(const SoundCommand& command)
{
    const SoundInfo& info = archive.sounds[command.soundNumber];
    if (info.kind != SoundKind::WaveSound || !MakeRoomInPlayer(info))
        return;

    StartWaveSound(command, info);
}

void SoundSystem::StartWaveSound(const SoundCommand& command, const SoundInfo& info)
{
    const LoadedWaveSoundFile& loaded = waveSoundFiles.at(info.fileId);
    const WaveSound& sound = loaded.file.sounds[info.waveSound.soundNumber];
    const NoteParameters& note = sound.note;

    VoiceSettings settings;
    settings.volume = float(info.volume) / HighestNumber * float(note.volume) / HighestNumber;
    settings.pan = PanFromNumber(sound.pan) + PanFromNumber(note.pan);
    settings.surroundPan = SurroundPanFromNumber(sound.surroundPan);
    settings.mainSend = float(sound.mainSend) / HighestNumber;
    settings.pitch = sound.pitch * note.tune;

    Envelope envelope(note.attack, note.decay, note.sustain, note.release);
    int priority = info.waveSound.channelPriority + info.playerPriority;
    std::optional<VoiceHandle> voice = voices.Start(loaded.waves[note.waveIndex], envelope, priority, settings);
    if (!voice)
        return;

    runningSounds.push_back({SoundHandle{command.handleNumber}, command.soundNumber, soundsStarted++, *voice});
}

// A player plays only so many of its sounds at once. When it is full, the sound with the lowest priority (the oldest of
// those) makes room, unless it is more important than the new sound.
bool SoundSystem::MakeRoomInPlayer(const SoundInfo& info)
{
    std::size_t soundsOfPlayer = 0;
    RunningSound* weakestSound = nullptr;

    for (RunningSound& sound : runningSounds)
    {
        const SoundInfo& runningInfo = archive.sounds[sound.soundNumber];
        if (runningInfo.playerNumber != info.playerNumber)
            continue;

        ++soundsOfPlayer;
        bool isWeaker = weakestSound == nullptr || runningInfo.playerPriority < archive.sounds[weakestSound->soundNumber].playerPriority;
        if (isWeaker)
            weakestSound = &sound;
    }

    if (soundsOfPlayer < archive.players[info.playerNumber].maximumSoundCount)
        return true;

    if (weakestSound == nullptr || archive.sounds[weakestSound->soundNumber].playerPriority > info.playerPriority)
        return false;

    std::uint32_t weakestHandleNumber = weakestSound->handle.number;
    StopRunningSound(*weakestSound);
    std::erase_if(runningSounds, [&](const RunningSound& sound) { return sound.handle.number == weakestHandleNumber; });
    return true;
}

void SoundSystem::StopRunningSound(RunningSound& sound)
{
    Voice* voice = voices.Find(sound.voice);
    if (voice != nullptr)
        voice->Release();
}

void SoundSystem::StartNextFrame()
{
    RunSoundFrame();

    std::uint32_t sampleRate = mixer.DeviceSampleRate();
    std::uint64_t firstSample = SamplesBeforeFrame(framesStarted, sampleRate);
    ++framesStarted;
    frameLength = static_cast<std::size_t>(SamplesBeforeFrame(framesStarted, sampleRate) - firstSample);
    samplesPlayedInFrame = 0;
}

void SoundSystem::RunSoundFrame()
{
    std::erase_if(runningSounds, [&](const RunningSound& sound) { return voices.Find(sound.voice) == nullptr; });
    voices.BeginFrame();
}

void SoundSystem::Render(std::span<float> interleavedStereo)
{
    ProcessCommands();

    if (paused.load())
    {
        std::fill(interleavedStereo.begin(), interleavedStereo.end(), 0.0f);
        return;
    }

    float volume = masterVolume.load();
    std::size_t totalSampleCount = interleavedStereo.size() / 2;
    std::size_t doneSampleCount = 0;

    while (doneSampleCount < totalSampleCount)
    {
        if (samplesPlayedInFrame == frameLength)
            StartNextFrame();

        std::size_t count = std::min(totalSampleCount - doneSampleCount, frameLength - samplesPlayedInFrame);
        std::span<float> left(leftSamples.data(), count);
        std::span<float> right(rightSamples.data(), count);
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);

        mixer.Mix(voices, left, right, samplesPlayedInFrame, frameLength);

        for (std::size_t sampleNumber = 0; sampleNumber < count; ++sampleNumber)
        {
            interleavedStereo[2 * (doneSampleCount + sampleNumber)] = left[sampleNumber] * volume;
            interleavedStereo[2 * (doneSampleCount + sampleNumber) + 1] = right[sampleNumber] * volume;
        }

        doneSampleCount += count;
        samplesPlayedInFrame += count;
    }
}

}
