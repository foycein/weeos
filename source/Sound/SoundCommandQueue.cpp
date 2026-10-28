#include "Sound/SoundCommandQueue.h"

namespace Sound
{

bool SoundCommandQueue::Push(const SoundCommand& command)
{
    std::size_t written = writtenCount.load(std::memory_order_relaxed);
    std::size_t read = readCount.load(std::memory_order_acquire);
    if (written - read == Capacity)
        return false;

    commands[written % Capacity] = command;
    writtenCount.store(written + 1, std::memory_order_release);
    return true;
}

bool SoundCommandQueue::Pop(SoundCommand& command)
{
    std::size_t read = readCount.load(std::memory_order_relaxed);
    std::size_t written = writtenCount.load(std::memory_order_acquire);
    if (read == written)
        return false;

    command = commands[read % Capacity];
    readCount.store(read + 1, std::memory_order_release);
    return true;
}

}
