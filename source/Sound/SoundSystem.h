#pragma once

#include "Sound/LoadedFiles.h"
#include "Sound/Mixer.h"
#include "Sound/SequencePlayer.h"
#include "Sound/SoundArchiveFile.h"
#include "Sound/SoundCommandQueue.h"
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
// Two threads work with it. The thread of the menu calls Play, Stop, StopAll, SetMasterVolume and SetPaused, one thread
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

    // Lets the sound fade out as its envelope says. Nothing happens when the sound has ended.
    void Stop(SoundHandle handle);
    void StopAll();

    // 1 is the volume of the sounds as they are in the archive. Louder than 1 may clip at the output.
    void SetMasterVolume(float volume);

    // A paused system makes silence and does not move on.
    void SetPaused(bool paused);

    // Fills the buffer with the next stereo samples, left and right alternating, at the sample rate of the device.
    void Render(std::span<float> interleavedStereo);

    std::size_t ActiveVoiceCount() const;
    std::size_t RunningSoundCount() const;

    // The number of times a track of a sequence had to be ended because of a command that is not supported (or that went wrong),
    // among all the sequences that have been played. The sequences of the menu never do.
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
    };

    void LoadWaveSoundFileOf(const SoundInfo& sound);
    void LoadSequenceFilesOf(const SoundInfo& sound);
    void ProcessCommands();
    void StartSound(const SoundCommand& command);
    void StartWaveSound(const SoundCommand& command, const SoundInfo& info);
    void StartSequenceSound(const SoundCommand& command, const SoundInfo& info);
    bool HasEnded(const RunningSound& sound);
    bool MakeRoomInPlayer(const SoundInfo& info);
    void StopRunningSound(RunningSound& sound);
    void RunSoundFrame();
    void StartNextFrame();

    SoundArchiveFile archive;
    std::map<std::uint32_t, LoadedWaveSoundFile> waveSoundFiles;
    std::map<std::uint32_t, LoadedBank> banks;
    std::map<std::uint32_t, SequenceFile> sequenceFiles;
    SequenceGlobals sequenceGlobals;
    std::size_t sequenceErrorCount = 0;
    VoicePool voices;
    Mixer mixer;
    SoundCommandQueue commands;
    std::vector<RunningSound> runningSounds;
    std::uint64_t soundsStarted = 0;

    std::atomic<std::uint32_t> lastHandleNumber{0};
    std::atomic<float> masterVolume{1.0f};
    std::atomic<bool> paused{false};

    // Where the audio is in time. A sound frame lasts 3 milliseconds, which is a whole number of samples only at some rates.
    std::uint64_t framesStarted = 0;
    std::size_t frameLength = 0;
    std::size_t samplesPlayedInFrame = 0;

    std::vector<float> leftSamples;
    std::vector<float> rightSamples;
};

}
