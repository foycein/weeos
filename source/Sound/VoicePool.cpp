#include "Sound/VoicePool.h"

namespace Sound
{

std::optional<std::size_t> VoicePool::ChooseSlot(int newPriority) const
{
    std::optional<std::size_t> weakestSlot;

    for (std::size_t slotNumber = 0; slotNumber < slots.size(); ++slotNumber)
    {
        const Slot& slot = slots[slotNumber];
        if (!slot.voice.IsActive())
            return slotNumber;

        bool isWeakerThanTheBest = !weakestSlot || slot.voice.Priority() < slots[*weakestSlot].voice.Priority() ||
            (slot.voice.Priority() == slots[*weakestSlot].voice.Priority() && slot.startNumber < slots[*weakestSlot].startNumber);
        if (isWeakerThanTheBest)
            weakestSlot = slotNumber;
    }

    if (weakestSlot && slots[*weakestSlot].voice.Priority() <= newPriority)
        return weakestSlot;

    return std::nullopt;
}

std::optional<VoiceHandle> VoicePool::Start(std::shared_ptr<const DecodedWave> wave, const Envelope& envelope, int priority, const VoiceSettings& settings)
{
    std::optional<std::size_t> chosenSlot = ChooseSlot(priority);
    if (!chosenSlot)
        return std::nullopt;

    Slot& slot = slots[*chosenSlot];
    ++slot.generation;
    slot.startNumber = voicesStarted++;
    slot.voice.Start(std::move(wave), envelope, priority, settings);
    return VoiceHandle{static_cast<std::uint32_t>(*chosenSlot), slot.generation};
}

Voice* VoicePool::Find(VoiceHandle handle)
{
    if (handle.slotNumber >= slots.size())
        return nullptr;

    Slot& slot = slots[handle.slotNumber];
    if (slot.generation != handle.generation || !slot.voice.IsActive())
        return nullptr;

    return &slot.voice;
}

void VoicePool::StopAll()
{
    for (Slot& slot : slots)
        slot.voice.Stop();
}

std::size_t VoicePool::ActiveVoiceCount() const
{
    std::size_t activeCount = 0;
    for (const Slot& slot : slots)
        activeCount += slot.voice.IsActive() ? 1 : 0;

    return activeCount;
}

void VoicePool::BeginFrame()
{
    for (Slot& slot : slots)
        slot.voice.BeginFrame();
}

void VoicePool::Mix(const Resampler& resampler, double deviceSampleRate, std::span<float> left, std::span<float> right,
    std::size_t firstSampleInFrame, std::size_t frameLength)
{
    for (Slot& slot : slots)
        slot.voice.Mix(resampler, deviceSampleRate, left, right, firstSampleInFrame, frameLength);
}

}
