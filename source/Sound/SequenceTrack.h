#pragma once

#include "Sound/NoteParameters.h"
#include "Sound/VoicePool.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Sound
{

constexpr std::size_t MaximumTrackCount = 16;
constexpr std::size_t VariableCount = 16;
constexpr std::size_t MaximumCallDepth = 3;

// The value of an envelope number of a track that was not set by a command. Then the instrument's number is used.
constexpr std::uint8_t EnvelopeNumberNotSet = 0xFF;

// A note that sounds. It is a voice, and what the track needs to know about it to keep the voice up to date.
struct PlayingNote
{
    VoiceHandle voice;

    // The number of ticks until the key is let go. -1 for a key that is held until the track ends.
    std::int32_t ticksLeft;

    int key;
    NoteParameters note;

    // How loud the note is from its velocity and the volume of its instrument. How far left or right the instrument puts it.
    float startVolume;
    float startPan;
};

// Where a call (or a repeated part) returns to. A part that repeats 0 times repeats for ever.
struct ReturnPoint
{
    std::size_t position;
    int repeatCount;
};

// One track of a sequence: its place in the commands, its waiting time, its settings and its notes.
// A sequence is up to 16 tracks that run at the same time.
struct SequenceTrack
{
    bool isAllocated = false;
    bool isRunning = false;

    // The place of the next command, counted from the first command of the sequence file.
    std::size_t position = 0;

    // The ticks to wait before the next command.
    int waitTicks = 0;

    // A note of length 0 holds the track back until all its notes have ended.
    bool isWaitingForNotes = false;

    // When this is on (the default), the track waits for the length of a note before it goes on.
    bool noteWait = true;

    // The instrument of the bank.
    std::uint16_t program = 0;

    // The loudness of the track from 0 to 127.
    std::uint8_t volume = 127;

    // From -1 (left) to 1 (right), and from 0 (front) to 2 (behind).
    float pan = 0.0f;
    float surroundPan = 0.0f;

    int transpose = 0;

    // The pitch is bent by this many 128th of the bend range of 2 semitones.
    int bend = 0;

    std::uint8_t priority = 64;
    std::uint8_t mainSend = 127;

    std::uint8_t attack = EnvelopeNumberNotSet;
    std::uint8_t decay = EnvelopeNumberNotSet;
    std::uint8_t sustain = EnvelopeNumberNotSet;
    std::uint8_t release = EnvelopeNumberNotSet;

    // Set by the commands that compare. The commands that follow the prefix "if" only run when it is not 0.
    std::uint8_t conditionFlag = 0;

    std::array<std::int16_t, VariableCount> variables;

    std::array<ReturnPoint, MaximumCallDepth> returnPoints{};
    std::size_t returnPointCount = 0;

    std::vector<PlayingNote> notes;

    SequenceTrack()
    {
        variables.fill(-1);
    }
};

}
