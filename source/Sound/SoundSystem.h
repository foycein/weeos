#pragma once

#include "Sound/FadingValue.h"
#include "Sound/LoadedFiles.h"
#include "Sound/Mixer.h"
#include "Sound/Reverb.h"
#include "Sound/SequencePlayer.h"
#include "Sound/SoundArchiveFile.h"
#include "Sound/SoundCommandQueue.h"
#include "Sound/SoundMode.h"
#include "Sound/VoicePool.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace Sound
{

// Names a sound that was started, so that it can be stopped. The handle with the number 0 names no sound.
struct SoundHandle
{
    std::uint32_t number = 0;

    bool IsValid() const
    {
        return number != 0;
    }
};

// Plays the sounds of a sound archive. The menu asks for a sound by its number in the archive and the system plays it
// into the buffers that the audio device asks for.
//
// Two threads work with it. The thread of the menu calls Play, Stop, ChangeVolume, the pauses, SetMasterVolume and SetPaused, one thread
// at a time. The audio thread calls Render. They talk through a queue without locks, so neither waits for the other.
class SoundSystem
{
public:
    // Reads all the files that the sounds of the archive use and decodes all their waves, so that playing a sound never has to
    // wait. This takes some tens of milliseconds. Throws a FormatError when a file is wrong.
    SoundSystem(SoundArchiveFile archiveToPlay, std::uint32_t deviceSampleRate);

    const SoundArchiveFile& Archive() const;

    // Starts a sound. The sound starts at the next call of Render, and the handle is given at once. When the player of the
    // sound is full of more important sounds, the sound is dropped at that time and Stop does nothing for its handle.
    // The handle names no sound when the number is not a sound of the archive or the queue to the audio thread is full.
    SoundHandle Play(std::size_t soundNumber);

    // The same, but the sound is dropped when a sound with the same number is still playing at that time.
    SoundHandle PlayIfNotPlaying(std::size_t soundNumber);

    // Lets the sound fade out as its envelope says. With a fade time, the sound first falls to silence in that time.
    // Nothing happens when the sound has ended.
    void Stop(SoundHandle handle, double fadeSeconds = 0.0);
    void StopAll(double fadeSeconds = 0.0);

    // Moves the volume of a sound that plays, from what it is now to the new volume, in the time given. 1 is the volume of the
    // archive, 0 is silence.
    void ChangeVolume(SoundHandle handle, float volume, double fadeSeconds);

    // The sounds that play now fall to silence in the time given and are then held where they are. Sounds that start later play.
    void PauseRunningSounds(double fadeSeconds);

    // The held sounds go on from where they were held and rise to their volume in the time given.
    void ResumePausedSounds(double fadeSeconds);

    // 1 is the volume of the sounds as they are in the archive. Louder than 1 may clip at the output. A change moves smoothly
    // over the next buffer that is rendered.
    void SetMasterVolume(float volume);
    float MasterVolume() const;

    // A paused system makes silence and does not move on.
    void SetPaused(bool paused);

    // The sound mode of the console, the setting IPL.SND. The sounds follow it from the next sound frame on.
    void SetSoundMode(SoundMode mode);

    // The reverb of the effect bus A, which the menu sets up. Without it the sounds are dry. Switched off, it forgets its tail.
    void SetReverbEnabled(bool enabled);
    bool IsReverbEnabled() const;

    // Tells the system that the device plays at another sample rate now. Sounds go on, but the tail of the reverb is lost.
    // Call it on the thread that calls Render.
    void SetDeviceSampleRate(std::uint32_t sampleRate);

    // Fills the buffer with the next stereo samples, left and right alternating, at the sample rate of the device.
    void Render(std::span<float> interleavedStereo);

    std::size_t ActiveVoiceCount() const;
    std::size_t RunningSoundCount() const;

    // True when a sound with this number of the archive plays. Ask it on the thread that calls Render.
    bool IsPlaying(std::size_t soundNumber) const;

    // The number of times something went wrong in a sequence (see SequencePlayer::ErrorCount), among all the sequences that have
    // been played. The sequences of the menu never do: every note they play has a sound in its bank.
    std::size_t SequenceErrorCount() const;

private:
    // A sound that plays: either a single voice (a wave sound) or a sequence that starts voices.
    struct RunningSound
    {
        SoundHandle handle;
        std::size_t soundNumber;
        std::uint64_t startNumber;
        VoiceHandle voice;
        std::unique_ptr<SequencePlayer> sequence;

        // The volume of the archive, and for a wave sound the settings of its voice at the start.
        float archiveVolume = 1.0f;
        VoiceSettings waveSettings;

        // What the menu does to the volume: its own volume, the fade of a stop and the fade of a pause.
        FadingValue volume;
        FadingValue stopFade;
        FadingValue pauseFade;
        bool stopsWhenFaded = false;
        bool pausesWhenFaded = false;
        bool isPaused = false;
        bool resumesAtNextFrame = false;
    };

    std::uint8_t PlayerPriorityOf(const RunningSound& sound) const;
    void LoadWaveSoundFileOf(const SoundInfo& sound);
    void LoadSequenceFilesOf(const SoundInfo& sound);
    void ProcessCommands();
    void CarryOut(const SoundCommand& command);
    RunningSound* FindRunningSound(std::uint32_t handleNumber);
    void StartStop(RunningSound& sound, int fadeFrames);
    void StartPause(RunningSound& sound, int fadeFrames);
    void Resume(RunningSound& sound, int fadeFrames);
    void HoldVoices(RunningSound& sound, bool shouldBeHeld);
    void UpdateFades(RunningSound& sound);
    void StartSound(const SoundCommand& command);
    void StartWaveSound(const SoundCommand& command, const SoundInfo& info);
    void StartSequenceSound(const SoundCommand& command, const SoundInfo& info);
    bool HasEnded(const RunningSound& sound);
    bool MakeRoomInPlayer(const SoundInfo& info);
    void StopRunningSound(RunningSound& sound);
    void RunSoundFrame();
    void StartNextFrame();
    void ResizeBuffers(std::uint32_t sampleRate);
    MixBuffers ClearedBuffers(std::size_t count);
    void AddReverb(const MixBuffers& buffers);

    SoundArchiveFile archive;
    std::map<std::uint32_t, LoadedWaveSoundFile> waveSoundFiles;
    std::map<std::uint32_t, LoadedBank> banks;
    std::map<std::uint32_t, SequenceFile> sequenceFiles;
    SequenceGlobals sequenceGlobals;
    std::size_t sequenceErrorCount = 0;
    VoicePool voices;
    Mixer mixer;
    Reverb reverb;
    SoundCommandQueue commands;
    std::vector<RunningSound> runningSounds;
    std::uint64_t soundsStarted = 0;

    std::atomic<std::uint32_t> lastHandleNumber{0};
    std::atomic<float> masterVolume{1.0f};

    // The master volume that the last rendered buffer ended with. The first buffer takes the master volume at once.
    float renderedMasterVolume = 1.0f;
    bool hasRendered = false;
    std::atomic<bool> paused{false};
    std::atomic<SoundMode> soundMode{SoundMode::Stereo};
    std::atomic<bool> reverbEnabled{true};

    // Where the audio is in time. A sound frame lasts 3 milliseconds, which is a whole number of samples only at some rates.
    std::uint64_t framesStarted = 0;
    std::size_t frameLength = 0;
    std::size_t samplesPlayedInFrame = 0;

    std::vector<float> leftSamples;
    std::vector<float> rightSamples;
    std::vector<float> reverbLeftSamples;
    std::vector<float> reverbRightSamples;
};

}
