#pragma once

#include <cstdint>

namespace Sound
{

// The numbers that an envelope is made of, for the parameters 0 to 127.

// How much of the distance to full loudness is left after one millisecond of the attack. 0 means the attack is over at once.
double AttackFactor(std::uint8_t attack);

// The level, in tenths of a decibel, at which the decay stops. Full is 0.
double SustainLevel(std::uint8_t sustain);

// How many tenths of a decibel the loudness falls in a millisecond during the decay or the release.
double FallRate(std::uint8_t parameter);

// How the loudness of a note rises, falls and dies away. The four numbers (0 to 127) come from the instrument or the sequence.
//
//   attack   the loudness rises from silence to full. 127 is at once, 0 is the slowest (many seconds).
//   decay    after the rise, the loudness falls to the sustain level. 127 is at once, 0 is the slowest.
//   sustain  the level that is held as long as the key is down. 127 is full, 0 is nearly silent.
//   release  after the key is let go, the loudness falls to silence. 127 is at once, 0 is the slowest.
//
// The loudness is kept in tenths of a decibel, from -904 (silence) up to 0 (full). Time is counted in milliseconds,
// and the owner moves the envelope on by 3 milliseconds for every sound frame.
class Envelope
{
public:
    Envelope(std::uint8_t attack, std::uint8_t decay, std::uint8_t sustain, std::uint8_t release);

    // Starts the fall to silence. Does nothing when the fall has started.
    void StartRelease();

    // Changes how fast the fall is, from now on.
    void SetRelease(std::uint8_t release);

    void Advance(int milliseconds);

    // How much of the full loudness is left, from 0 (silence) to 1 (full).
    float Gain() const;

    // True when the release has made the note silent.
    bool IsFinished() const;

private:
    enum class Stage
    {
        Attack,
        Decay,
        Sustain,
        Release,
    };

    Stage stage;
    double decibelTenths;
    double attackFactor;
    double decayRate;
    double sustainLevel;
    double releaseRate;
};

}
