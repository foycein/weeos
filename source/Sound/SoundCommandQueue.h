#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Sound
{

enum class SoundCommandKind : std::uint8_t
{
    Play,
    Stop,
    StopAll,
};

struct SoundCommand
{
    SoundCommandKind kind;

    // The number of the handle that the sound is known by. Used by Play and Stop.
    std::uint32_t handleNumber;

    // The number of the sound in the archive. Used by Play.
    std::size_t soundNumber;
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
