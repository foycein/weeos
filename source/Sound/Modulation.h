#pragma once

#include <cstdint>

namespace Sound
{

// What a modulation moves. A wobbling pitch is a vibrato, a wobbling volume a tremolo, and the third wobbles the pan.
enum class ModulationTarget : std::uint8_t
{
    Pitch,
    Volume,
    Pan,

    // A target that the library does not know: the wave goes round and changes nothing.
    None,
};

// The sequences name the target by a number: 0, 1 and 2 are the targets above, any other number is None.
ModulationTarget ModulationTargetFromNumber(std::uint8_t number);

// The speed a track starts with, in cycles per second.
constexpr float StartModulationSpeed = 6.25f;

// The wave is a sine that is looked up in 128 steps for each cycle, with whole numbers up to 127 (0x8161e658 and 0x815015f0).
constexpr int StepsInOneCycle = 128;
constexpr float HighestSineNumber = 127.0f;

// The whole number, from -127 to 127, that the wave has at a step of its cycle.
int ModulationSineNumber(int step);

// The modulation of the notes of a track, as the commands of the track set it. A track starts with no depth, so with no modulation.
struct ModulationSettings
{
    // How big the wave is, as a part of the range.
    float depth = 0.0f;

    // The cycles of the wave in a second.
    float speed = StartModulationSpeed;

    // The time from the start of a note to the start of the wave.
    int delayMilliseconds = 0;

    // The size of the wave at depth 1. For the pitch it is in semitones, for the volume in steps of 6 decibels, for the pan in
    // widths of the whole stereo picture from the middle to one side.
    std::uint8_t range = 1;

    ModulationTarget target = ModulationTarget::Pitch;
};

// The wave of the modulation of one note. It starts when the note starts, waits for the delay, and then goes round at the speed.
class Modulation
{
public:
    // How far the wave is from the middle now, in the unit of the target (see ModulationSettings::range). 0 before the delay is over.
    float Value(const ModulationSettings& settings) const;

    // Moves the wave on. The track can change its settings while a note sounds, so they are given at every call.
    void Advance(const ModulationSettings& settings, int milliseconds);

private:
    int elapsedMilliseconds = 0;

    // Where the wave is in its cycle, from 0 up to but not including 1.
    float cyclePosition = 0.0f;
};

}
