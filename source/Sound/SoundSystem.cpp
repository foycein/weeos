#include "Sound/SoundSystem.h"

#include "Sound/StereoPlacement.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <cmath>
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

// A change of the master volume moves in a straight line over one rendered buffer. A volume that does not change is exact.
float MasterVolumeAt(std::size_t samplesDone, std::size_t sampleCount, float startVolume, float endVolume)
{
    if (startVolume == endVolume)
        return endVolume;

    float fractionDone = float(samplesDone) / float(sampleCount);
    return startVolume + (endVolume - startVolume) * fractionDone;
}

// A wave of PlayWave is no sound of the archive, so it has no number of one.
constexpr std::size_t FreeWaveNumber = static_cast<std::size_t>(-1);

// A wave of PlayWave starts at full loudness at once and ends at once when it is let go, and it is as important as the most important
// sound of the menu, so that it keeps its voice.
constexpr std::uint8_t ImmediateEnvelopeNumber = 127;
constexpr int FreeWavePriority = 127;

int SoundFramesOf(double seconds)
{
    double frames = seconds * double(MillisecondsPerSecond) / double(SoundFrameMilliseconds);
    return std::max(0, int(std::lround(frames)));
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
    return Play(soundNumber, SoundStartSettings{});
}

SoundHandle SoundSystem::Play(std::size_t soundNumber, const SoundStartSettings& startSettings)
{
    if (soundNumber >= archive.sounds.size())
        return SoundHandle{};

    SoundHandle handle{++lastHandleNumber};
    SoundCommand command{SoundCommandKind::Play, handle.number, soundNumber};
    command.startSettings = startSettings;
    if (!commands.Push(command))
        return SoundHandle{};

    return handle;
}

SoundHandle SoundSystem::PlayIfNotPlaying(std::size_t soundNumber)
{
    if (soundNumber >= archive.sounds.size())
        return SoundHandle{};

    SoundHandle handle{++lastHandleNumber};
    if (!commands.Push({SoundCommandKind::PlayIfNotPlaying, handle.number, soundNumber}))
        return SoundHandle{};

    return handle;
}

SoundHandle SoundSystem::PlayWave(std::shared_ptr<const DecodedWave> wave, float volume)
{
    if (!wave || wave->channels.empty() || wave->channels.size() > Voice::MaximumChannelCount)
        return SoundHandle{};

    SoundHandle handle{++lastHandleNumber};
    SoundCommand command{SoundCommandKind::PlayWave, handle.number, FreeWaveNumber};
    command.volume = volume;
    command.wave = std::move(wave);
    if (!commands.Push(command))
        return SoundHandle{};

    return handle;
}

void SoundSystem::Stop(SoundHandle handle, double fadeSeconds)
{
    SoundCommand command{SoundCommandKind::Stop, handle.number, 0};
    command.fadeFrames = SoundFramesOf(fadeSeconds);
    commands.Push(command);
}

void SoundSystem::StopAll(double fadeSeconds)
{
    SoundCommand command{SoundCommandKind::StopAll, 0, 0};
    command.fadeFrames = SoundFramesOf(fadeSeconds);
    commands.Push(command);
}

void SoundSystem::ChangeVolume(SoundHandle handle, float volume, double fadeSeconds)
{
    SoundCommand command{SoundCommandKind::ChangeVolume, handle.number, 0};
    command.volume = volume;
    command.fadeFrames = SoundFramesOf(fadeSeconds);
    commands.Push(command);
}

void SoundSystem::PauseRunningSounds(double fadeSeconds)
{
    SoundCommand command{SoundCommandKind::PauseRunningSounds, 0, 0};
    command.fadeFrames = SoundFramesOf(fadeSeconds);
    commands.Push(command);
}

void SoundSystem::ResumePausedSounds(double fadeSeconds)
{
    SoundCommand command{SoundCommandKind::ResumePausedSounds, 0, 0};
    command.fadeFrames = SoundFramesOf(fadeSeconds);
    commands.Push(command);
}

void SoundSystem::SetMasterVolume(float volume)
{
    masterVolume.store(volume);
}

float SoundSystem::MasterVolume() const
{
    return masterVolume.load();
}

void SoundSystem::SetConsoleVolume(float volume)
{
    consoleVolume.store(volume);
}

float SoundSystem::ConsoleVolume() const
{
    return consoleVolume.load();
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
        CarryOut(command);
}

void SoundSystem::CarryOut(const SoundCommand& command)
{
    RunningSound* sound = FindRunningSound(command.handleNumber);

    switch (command.kind)
    {
    case SoundCommandKind::Play:
        StartSound(command);
        return;

    case SoundCommandKind::PlayIfNotPlaying:
        if (!IsPlaying(command.soundNumber))
            StartSound(command);
        return;

    case SoundCommandKind::PlayWave:
        StartFreeWave(command);
        return;

    case SoundCommandKind::Stop:
        if (sound != nullptr)
            StartStop(*sound, command.fadeFrames);
        return;

    case SoundCommandKind::StopAll:
        for (RunningSound& runningSound : runningSounds)
            StartStop(runningSound, command.fadeFrames);
        return;

    case SoundCommandKind::ChangeVolume:
        if (sound != nullptr)
            sound->volume.MoveTo(command.volume, command.fadeFrames);
        return;

    case SoundCommandKind::PauseRunningSounds:
        for (RunningSound& runningSound : runningSounds)
            StartPause(runningSound, command.fadeFrames);
        return;

    case SoundCommandKind::ResumePausedSounds:
        for (RunningSound& runningSound : runningSounds)
            Resume(runningSound, command.fadeFrames);
        return;
    }
}

bool SoundSystem::IsPlaying(std::size_t soundNumber) const
{
    for (const RunningSound& sound : runningSounds)
    {
        if (sound.soundNumber == soundNumber)
            return true;
    }
    return false;
}

SoundSystem::RunningSound* SoundSystem::FindRunningSound(std::uint32_t handleNumber)
{
    for (RunningSound& sound : runningSounds)
    {
        if (sound.handle.number == handleNumber)
            return &sound;
    }
    return nullptr;
}

// A stop without a fade lets the sound go at once, as its envelope says. With a fade, the sound first falls to silence.
void SoundSystem::StartStop(RunningSound& sound, int fadeFrames)
{
    if (fadeFrames == 0)
    {
        StopRunningSound(sound);
        return;
    }

    sound.stopFade.MoveTo(0.0f, fadeFrames);
    sound.stopsWhenFaded = true;
}

// A sound that is paused already stays paused, also when it was about to go on.
void SoundSystem::StartPause(RunningSound& sound, int fadeFrames)
{
    if (sound.stopsWhenFaded)
        return;

    sound.resumesAtNextFrame = false;
    sound.pauseFade.MoveTo(0.0f, fadeFrames);
    sound.pausesWhenFaded = !sound.isPaused;
}

// The voices are let go at the start of the next sound frame, so that a sound stands still for whole frames.
void SoundSystem::Resume(RunningSound& sound, int fadeFrames)
{
    if (!sound.isPaused && !sound.pausesWhenFaded)
        return;

    sound.resumesAtNextFrame = sound.isPaused;
    sound.pausesWhenFaded = false;
    sound.pauseFade.MoveTo(1.0f, fadeFrames);
}

// A paused sound holds its voices, and a sequence also stands still.
void SoundSystem::HoldVoices(RunningSound& sound, bool shouldBeHeld)
{
    sound.isPaused = shouldBeHeld;
    if (sound.sequence)
    {
        sound.sequence->SetHeld(shouldBeHeld);
        return;
    }

    Voice* voice = voices.Find(sound.voice);
    if (voice != nullptr)
        voice->SetHeld(shouldBeHeld);
}

// Moves the fades of a sound on by one sound frame and gives its voices the volume that comes out. A stop or a pause whose fade has
// reached silence is carried out then.
void SoundSystem::UpdateFades(RunningSound& sound)
{
    if (sound.resumesAtNextFrame)
    {
        sound.resumesAtNextFrame = false;
        HoldVoices(sound, false);
    }

    sound.volume.Advance();
    sound.stopFade.Advance();
    sound.pauseFade.Advance();

    float fadeVolume = sound.volume.Value() * sound.stopFade.Value() * sound.pauseFade.Value();
    if (sound.sequence)
    {
        sound.sequence->SetVolume(sound.archiveVolume * fadeVolume);
    }
    else
    {
        VoiceSettings settings = sound.waveSettings;
        settings.volume *= fadeVolume;
        Voice* voice = voices.Find(sound.voice);
        if (voice != nullptr)
            voice->SetSettings(settings);
    }

    if (sound.stopsWhenFaded && sound.stopFade.HasArrived())
    {
        sound.stopsWhenFaded = false;
        StopRunningSound(sound);
    }

    if (sound.pausesWhenFaded && sound.pauseFade.HasArrived())
    {
        sound.pausesWhenFaded = false;
        HoldVoices(sound, true);
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

    RunningSound* started = FindRunningSound(command.handleNumber);
    if (started == nullptr)
        return;

    const SoundStartSettings& startSettings = command.startSettings;
    started->volume.MoveTo(startSettings.volume, 0);
    started->playerPriority = startSettings.playerPriority.value_or(info.playerPriority);
    if (started->sequence)
    {
        started->sequence->SetPan(startSettings.pan);
        started->sequence->SetPitch(startSettings.pitch);
    }
}

void SoundSystem::StartSequenceSound(const SoundCommand& command, const SoundInfo& info)
{
    const SequenceFile& sequenceFile = sequenceFiles.at(info.fileId);
    const LoadedBank& bank = banks.at(archive.banks[info.sequence.bankNumber].fileId);

    float soundVolume = float(info.volume) / HighestNumber;
    RunningSound sound{SoundHandle{command.handleNumber}, command.soundNumber, soundsStarted, VoiceHandle{}, nullptr};
    ++soundsStarted;
    sound.sequence = std::make_unique<SequencePlayer>(sequenceFile, bank, info.sequence, soundVolume, voices, sequenceGlobals);
    sound.archiveVolume = soundVolume;
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
    settings.pan += command.startSettings.pan;
    settings.pitch *= command.startSettings.pitch;

    // The volume of the start is kept apart, in the volume of the running sound; the voice has it from its first sample.
    VoiceSettings firstSettings = settings;
    firstSettings.volume *= command.startSettings.volume;

    Envelope envelope(note.attack, note.decay, note.sustain, note.release);
    int priority = info.waveSound.channelPriority + info.playerPriority;
    std::optional<VoiceHandle> voice = voices.Start(loaded.waves[note.waveIndex], envelope, priority, firstSettings);
    if (!voice)
        return;

    RunningSound running{SoundHandle{command.handleNumber}, command.soundNumber, soundsStarted, *voice, nullptr};
    ++soundsStarted;
    running.waveSettings = settings;
    runningSounds.push_back(std::move(running));
}

std::uint8_t SoundSystem::PlayerPriorityOf(const RunningSound& sound) const
{
    return sound.playerPriority;
}

void SoundSystem::StartFreeWave(const SoundCommand& command)
{
    VoiceSettings settings;
    settings.volume = command.volume;

    Envelope envelope(ImmediateEnvelopeNumber, ImmediateEnvelopeNumber, ImmediateEnvelopeNumber, ImmediateEnvelopeNumber);
    std::optional<VoiceHandle> voice = voices.Start(command.wave, envelope, FreeWavePriority, settings);
    if (!voice)
        return;

    RunningSound running{SoundHandle{command.handleNumber}, FreeWaveNumber, soundsStarted, *voice, nullptr};
    ++soundsStarted;
    running.waveSettings = settings;
    runningSounds.push_back(std::move(running));
}

// A player plays only so many of its sounds at once. When it is full, the sound with the lowest priority (the oldest of
// those) makes room, unless it is more important than the new sound.
bool SoundSystem::MakeRoomInPlayer(const SoundInfo& info)
{
    std::size_t soundsOfPlayer = 0;
    RunningSound* weakestSound = nullptr;

    for (RunningSound& sound : runningSounds)
    {
        if (sound.soundNumber == FreeWaveNumber)
            continue;

        const SoundInfo& runningInfo = archive.sounds[sound.soundNumber];
        if (runningInfo.playerNumber != info.playerNumber)
            continue;

        ++soundsOfPlayer;
        bool isWeaker = weakestSound == nullptr || PlayerPriorityOf(sound) < PlayerPriorityOf(*weakestSound);
        if (isWeaker)
            weakestSound = &sound;
    }

    if (soundsOfPlayer < archive.players[info.playerNumber].maximumSoundCount)
        return true;

    if (weakestSound == nullptr || PlayerPriorityOf(*weakestSound) > info.playerPriority)
        return false;

    std::uint32_t weakestHandleNumber = weakestSound->handle.number;
    StopRunningSound(*weakestSound);
    if (weakestSound->sequence)
        sequenceErrorCount += weakestSound->sequence->ErrorCount();
    std::erase_if(runningSounds, [&](const RunningSound& sound) { return sound.handle.number == weakestHandleNumber; });
    return true;
}

// A paused sound is let go first, so that its voices can end.
void SoundSystem::StopRunningSound(RunningSound& sound)
{
    sound.resumesAtNextFrame = false;
    if (sound.isPaused)
        HoldVoices(sound, false);

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
        UpdateFades(sound);
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
// anything, because its tail is still sounding. A reverb that is switched off loses its tail, as an effect that the menu shuts down
// does, and starts from an empty room when it is switched on again.
void SoundSystem::AddReverb(const MixBuffers& buffers)
{
    if (!reverbEnabled.load())
    {
        reverb.Clear();
        return;
    }

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

bool SoundSystem::IsReverbEnabled() const
{
    return reverbEnabled.load();
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

    float endVolume = masterVolume.load() * consoleVolume.load();
    float startVolume = hasRendered ? renderedMasterVolume : endVolume;
    renderedMasterVolume = endVolume;
    hasRendered = true;

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
            std::size_t outputNumber = doneSampleCount + sampleNumber;
            float volume = MasterVolumeAt(outputNumber + 1, totalSampleCount, startVolume, endVolume);
            interleavedStereo[StereoChannelCount * outputNumber] = left[sampleNumber] * volume;
            interleavedStereo[StereoChannelCount * outputNumber + 1] = right[sampleNumber] * volume;
        }

        doneSampleCount += count;
        samplesPlayedInFrame += count;
    }
}

}
