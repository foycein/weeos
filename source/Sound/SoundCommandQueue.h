#pragma once

#include "Sound/SoundStartSettings.h"
#include "Sound/WaveDecoder.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Sound
{

enum class SoundCommandKind : std::uint8_t
{
    Play,
    PlayIfNotPlaying,
    PlayWave,
    Stop,
    StopAll,
    ChangeVolume,
    PauseRunningSounds,
    ResumePausedSounds,
};

struct SoundCommand
{
    SoundCommandKind kind;

    // The number of the handle that the sound is known by. Used by Play, PlayIfNotPlaying, Stop and ChangeVolume.
    std::uint32_t handleNumber;

    // The number of the sound in the archive. Used by Play and PlayIfNotPlaying.
    std::size_t soundNumber;

    // The new volume of ChangeVolume, and the volume of PlayWave.
    float volume = 1.0f;

    // What Play changes about the sound it starts.
    SoundStartSettings startSettings;

    // The wave that PlayWave plays.
    std::shared_ptr<const DecodedWave> wave;

    // How many sound frames a stop, a change of the volume, a pause or a resume takes. 0 is at once.
    int fadeFrames = 0;
};

// Hands commands from the thread of the menu to the audio thread without locks. One thread may push and one may pop.
class SoundCommandQueue
{
public:
    // False when the queue is full. The command is not kept then.
    bool Push(const SoundCommand& command);

    // False when there is no command.
    bool Pop(SoundCommand& command);

private:
    static constexpr std::size_t Capacity = 256;

    std::array<SoundCommand, Capacity> commands{};

    // Counts all the commands ever written and ever read. The place of a command is the count modulo the capacity.
    std::atomic<std::size_t> writtenCount{0};
    std::atomic<std::size_t> readCount{0};
};

}
