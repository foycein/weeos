#pragma once

#include "Sound/Voice.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace Sound
{

// Names one voice of the pool. A handle stays valid only as long as the voice plays the sound it was started for.
// When the voice has ended, or was taken over by a more important sound, the pool no longer finds it.
struct VoiceHandle
{
    std::uint32_t slotNumber;
    std::uint32_t generation;
};

// The voices that can play at the same time. The console has 64.
class VoicePool
{
public:
    static constexpr std::size_t MaximumVoiceCount = 64;

    // Starts a voice. When all voices are busy, the voice with the lowest priority is stopped for it (the oldest of
    // those, if several have the lowest priority), unless that voice has a higher priority than the new one.
    // Then the new voice is not started and the result is empty.
    std::optional<VoiceHandle> Start(
        std::shared_ptr<const DecodedWave> wave, const Envelope& envelope, int priority, const VoiceSettings& settings);

    // Gives the voice, or nothing when the voice has ended or was taken over.
    Voice* Find(VoiceHandle handle);

    void StopAll();

    std::size_t ActiveVoiceCount() const;

    // Moves every voice on to the next sound frame.
    void BeginFrame(SoundMode mode);

    // Adds every voice to the buffers. See Voice::Mix.
    void Mix(const Resampler& resampler, double deviceSampleRate, const MixBuffers& buffers, std::size_t firstSampleInFrame,
        std::size_t frameLength);

private:
    struct Slot
    {
        Voice voice;
        std::uint32_t generation = 1;
        std::uint64_t startNumber = 0;
    };

    // The number of a free slot, or of the slot whose voice may be stopped, or nothing.
    std::optional<std::size_t> ChooseSlot(int newPriority) const;

    // A voice of a lower priority is weaker. Of two voices with the same priority, the one that started first is weaker.
    static bool IsWeaker(const Slot& slot, const Slot& other);

    std::array<Slot, MaximumVoiceCount> slots;
    std::uint64_t voicesStarted = 0;
};

}
