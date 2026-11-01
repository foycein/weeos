#include "Sound/SoundSystem.h"

#include "Sound/StereoPlacement.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <format>

namespace Sound
{

namespace
{

constexpr float HighestNumber = 127.0f;
constexpr std::uint64_t MillisecondsPerSecond = 1000;

// The AX library plays what an effect makes from a frame of its bus two sound frames later: three buffers take turns, one that the
// voices fill, one that the effect works on and one that is played (0x81552880, 0x81552690, 0x815526c4).
constexpr std::uint64_t EffectBusDelayFrames = 2;

std::size_t EffectBusDelaySamples(std::uint32_t sampleRate)
{
    return std::size_t(sampleRate * EffectBusDelayFrames * SoundFrameMilliseconds / MillisecondsPerSecond);
}

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
    , reverb(deviceSampleRate, MenuReverbParameters, EffectBusDelaySamples(deviceSampleRate))
{
    for (const SoundInfo& sound : archive.sounds)
    {
        if (sound.kind == SoundKind::WaveSound)
            LoadWaveSoundFileOf(sound);
        else
            LoadSequenceFilesOf(sound);
    }

    ResizeBuffers(deviceSampleRate);
}

void SoundSystem::LoadWaveSoundFileOf(const SoundInfo& sound)
{
    if (!waveSoundFiles.contains(sound.fileId))
        waveSoundFiles.emplace(sound.fileId, LoadWaveSoundFile(archive, sound.fileId));

    const LoadedWaveSoundFile& loaded = waveSoundFiles.at(sound.fileId);
    if (sound.waveSound.soundNumber >= loaded.file.sounds.size())
    {
        throw Formats::FormatError(std::format("Sound system: the sound {} is the sound {} of the file {}, which has {} sounds", sound.name,
            sound.waveSound.soundNumber, sound.fileId, loaded.file.sounds.size()));
    }
}

void SoundSystem::LoadSequenceFilesOf(const SoundInfo& sound)
{
    if (!sequenceFiles.contains(sound.fileId))
        sequenceFiles.emplace(sound.fileId, LoadSequenceFile(archive, sound.fileId));

    std::uint32_t bankFileId = archive.banks[sound.sequence.bankNumber].fileId;
    if (!banks.contains(bankFileId))
        banks.emplace(bankFileId, LoadBank(archive, bankFileId));
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

std::size_t SoundSystem::SequenceErrorCount() const
{
    std::size_t errorCount = sequenceErrorCount;
    for (const RunningSound& sound : runningSounds)
        errorCount += sound.sequence ? sound.sequence->ErrorCount() : 0;

    return errorCount;
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
    if (!MakeRoomInPlayer(info))
        return;

    if (info.kind == SoundKind::WaveSound)
        StartWaveSound(command, info);
    else
        StartSequenceSound(command, info);
}

void SoundSystem::StartSequenceSound(const SoundCommand& command, const SoundInfo& info)
{
    const SequenceFile& sequenceFile = sequenceFiles.at(info.fileId);
    const LoadedBank& bank = banks.at(archive.banks[info.sequence.bankNumber].fileId);

    float soundVolume = float(info.volume) / HighestNumber;
    RunningSound sound{SoundHandle{command.handleNumber}, command.soundNumber, soundsStarted++, VoiceHandle{}, nullptr};
    sound.sequence = std::make_unique<SequencePlayer>(sequenceFile, bank, info.sequence, soundVolume, voices, sequenceGlobals);
    runningSounds.push_back(std::move(sound));
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
    settings.effectSend = float(sound.effectSendA) / HighestNumber;
    settings.pitch = sound.pitch * note.tune;

    Envelope envelope(note.attack, note.decay, note.sustain, note.release);
    int priority = info.waveSound.channelPriority + info.playerPriority;
    std::optional<VoiceHandle> voice = voices.Start(loaded.waves[note.waveIndex], envelope, priority, settings);
    if (!voice)
        return;

    runningSounds.push_back({SoundHandle{command.handleNumber}, command.soundNumber, soundsStarted++, *voice, nullptr});
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
    if (weakestSound->sequence)
        sequenceErrorCount += weakestSound->sequence->ErrorCount();
    std::erase_if(runningSounds, [&](const RunningSound& sound) { return sound.handle.number == weakestHandleNumber; });
    return true;
}

void SoundSystem::StopRunningSound(RunningSound& sound)
{
    if (sound.sequence)
    {
        sound.sequence->Stop();
        return;
    }

    Voice* voice = voices.Find(sound.voice);
    if (voice != nullptr)
        voice->Release();
}

// A wave sound has ended when its voice has. A sequence has ended when its tracks have ended, while its last voices may still fade out.
bool SoundSystem::HasEnded(const RunningSound& sound)
{
    if (sound.sequence)
        return sound.sequence->HasEnded();

    return voices.Find(sound.voice) == nullptr;
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
    for (RunningSound& sound : runningSounds)
    {
        if (sound.sequence)
            sound.sequence->Update();
    }

    for (const RunningSound& sound : runningSounds)
    {
        if (sound.sequence && HasEnded(sound))
            sequenceErrorCount += sound.sequence->ErrorCount();
    }
    std::erase_if(runningSounds, [&](const RunningSound& sound) { return HasEnded(sound); });

    sequenceGlobals.NextRandomNumber();
    voices.BeginFrame(soundMode.load());
}

MixBuffers SoundSystem::ClearedBuffers(std::size_t count)
{
    MixBuffers buffers{
        std::span<float>(leftSamples.data(), count),
        std::span<float>(rightSamples.data(), count),
        std::span<float>(reverbLeftSamples.data(), count),
        std::span<float>(reverbRightSamples.data(), count),
    };

    std::fill(buffers.left.begin(), buffers.left.end(), 0.0f);
    std::fill(buffers.right.begin(), buffers.right.end(), 0.0f);
    std::fill(buffers.reverbLeft.begin(), buffers.reverbLeft.end(), 0.0f);
    std::fill(buffers.reverbRight.begin(), buffers.reverbRight.end(), 0.0f);
    return buffers;
}

// The reverb gets what the voices send to it, and what comes out is added to the speakers. It also goes on when no voice sends
// anything, because its tail is still sounding.
void SoundSystem::AddReverb(const MixBuffers& buffers)
{
    if (!reverbEnabled.load())
        return;

    reverb.Process(buffers.reverbLeft, buffers.reverbRight);
    for (std::size_t sampleNumber = 0; sampleNumber < buffers.left.size(); ++sampleNumber)
    {
        buffers.left[sampleNumber] += buffers.reverbLeft[sampleNumber];
        buffers.right[sampleNumber] += buffers.reverbRight[sampleNumber];
    }
}

void SoundSystem::SetSoundMode(SoundMode mode)
{
    soundMode.store(mode);
}

void SoundSystem::SetReverbEnabled(bool enabled)
{
    reverbEnabled.store(enabled);
}

void SoundSystem::SetDeviceSampleRate(std::uint32_t sampleRate)
{
    if (sampleRate == mixer.DeviceSampleRate())
        return;

    mixer.SetDeviceSampleRate(sampleRate);
    reverb = Reverb(sampleRate, MenuReverbParameters, EffectBusDelaySamples(sampleRate));
    ResizeBuffers(sampleRate);

    // The frames start again from the time zero, because the grid of the frames depends on the rate.
    framesStarted = 0;
    frameLength = 0;
    samplesPlayedInFrame = 0;
}

void SoundSystem::ResizeBuffers(std::uint32_t sampleRate)
{
    leftSamples.resize(LongestFrameLength(sampleRate));
    rightSamples.resize(LongestFrameLength(sampleRate));
    reverbLeftSamples.resize(LongestFrameLength(sampleRate));
    reverbRightSamples.resize(LongestFrameLength(sampleRate));
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
    std::size_t totalSampleCount = interleavedStereo.size() / StereoChannelCount;
    std::size_t doneSampleCount = 0;

    while (doneSampleCount < totalSampleCount)
    {
        if (samplesPlayedInFrame == frameLength)
            StartNextFrame();

        std::size_t count = std::min(totalSampleCount - doneSampleCount, frameLength - samplesPlayedInFrame);
        MixBuffers buffers = ClearedBuffers(count);

        mixer.Mix(voices, buffers, samplesPlayedInFrame, frameLength);
        AddReverb(buffers);

        std::span<float> left = buffers.left;
        std::span<float> right = buffers.right;
        for (std::size_t sampleNumber = 0; sampleNumber < count; ++sampleNumber)
        {
            std::size_t firstChannelIndex = StereoChannelCount * (doneSampleCount + sampleNumber);
            interleavedStereo[firstChannelIndex] = left[sampleNumber] * volume;
            interleavedStereo[firstChannelIndex + 1] = right[sampleNumber] * volume;
        }

        doneSampleCount += count;
        samplesPlayedInFrame += count;
    }
}

}
