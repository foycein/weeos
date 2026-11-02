#pragma once

#include <cstdint>
#include <optional>

namespace Sound
{

// What the one who starts a sound of the archive changes about it, on top of what the archive says: the volume, pan, pitch and
// player priority that the original sets on the handle of a sound it has just started (the steps of the Miis, 80018F3C).
struct SoundStartSettings
{
    // Multiplies the volume of the archive: 1 is full, 0 is silence.
    float volume = 1.0f;

    // Added to the pan of every voice: -1 is the left, 0 the middle and 1 the right.
    float pan = 0.0f;

    // Multiplies the pitch of every voice: 2 is an octave up.
    float pitch = 1.0f;

    // The priority the sound keeps in its player once it plays. It still has to win its place with the priority of the archive.
    std::optional<std::uint8_t> playerPriority;
};

}
