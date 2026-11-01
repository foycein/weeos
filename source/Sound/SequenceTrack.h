#pragma once

#include "Sound/Envelope.h"
#include "Sound/Modulation.h"
#include "Sound/NoteParameters.h"
#include "Sound/VoicePool.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Sound
{

constexpr std::size_t MaximumTrackCount = 16;
constexpr std::size_t VariableCount = 16;
constexpr std::size_t MaximumCallDepth = 3;

// Every variable starts with this value.
constexpr std::int16_t StartVariableValue = -1;

// The length of a note that is held until the track lets it go.
constexpr std::int32_t HeldUntilReleased = -1;

// What a track starts with, before a command changes it (0x81505200).
constexpr std::uint8_t StartTrackVolume = 127;
constexpr std::uint8_t StartExpression = 127;
constexpr std::uint8_t StartTrackPriority = 64;
constexpr std::uint8_t StartMainSend = 127;
constexpr std::uint8_t StartBendRange = 2;
constexpr std::uint8_t StartLowPassCutoff = 64;
constexpr int StartPortamentoKey = 60;

// The pitch of a note can start away from the pitch of its key and slide to it. The slide is a straight line in semitones.
struct PitchSweep
{
    // How far from the key the pitch is at the start of the slide. 0 is no slide.
    float semitones = 0.0f;

    // How long the slide lasts and how far it has got. The unit is a tick, or a millisecond when countsMilliseconds is set.
    int length = 0;
    int position = 0;
    bool countsMilliseconds = false;

    void Advance(int amount)
    {
        position = std::min(position + amount, length);
    }

    // The semitones that the pitch is still away from its key.
    float SemitonesLeft() const
    {
        if (semitones == 0.0f || position >= length)
            return 0.0f;

        return semitones * float(length - position) / float(length);
    }
};

// A note that sounds. It is a voice, and what the track needs to know about it to keep the voice up to date.
struct PlayingNote
{
    VoiceHandle voice;

    // The number of ticks until the key is let go, or HeldUntilReleased.
    std::int32_t ticksLeft;

    int key;
    NoteParameters parameters;

    // How loud the note is from its velocity and the volume of its instrument. How far left or right the instrument puts it.
    float startVolume;
    float startPan;

    PitchSweep sweep;
    Modulation modulation;
};

// A call or a loop remembers the place to go back to. A loop also counts its rounds: 0 is a loop that never ends.
struct ReturnFrame
{
    std::size_t position = 0;
    std::uint8_t loopCount = 0;
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

    // The loudness of the track from 0 to 127. The expression is a second loudness, so that a sequence can fade a track
    // without touching the volume it set.
    std::uint8_t volume = StartTrackVolume;
    std::uint8_t expression = StartExpression;

    // From -1 (left) to 1 (right), and from 0 (front) to 2 (behind).
    float pan = 0.0f;
    float surroundPan = 0.0f;

    // The pan that a note starts with, on top of the pan of its instrument.
    float initialPan = 0.0f;

    int transpose = 0;

    // The pitch is bent by this many 128th of the bend range, which is given in semitones.
    int bend = 0;
    std::uint8_t bendRange = StartBendRange;

    std::uint8_t priority = StartTrackPriority;
    std::uint8_t mainSend = StartMainSend;

    // How much of the notes goes to the reverb of the effect bus A, 0 to 127.
    std::uint8_t effectSendA = 0;

    std::uint8_t lowPassCutoff = StartLowPassCutoff;

    // A muted track does not start notes.
    bool isMuted = false;

    // While this is on, a new note takes over the voice of the note before it instead of starting a voice.
    bool isTied = false;

    // While this is on, a note slides to its key from the key of the note before it (the time is the portamento time).
    bool isPortamentoOn = false;
    int portamentoKey = StartPortamentoKey;
    std::uint8_t portamentoTime = 0;

    // The slide that every note starts with, in semitones.
    float sweepSemitones = 0.0f;

    ModulationSettings modulation;

    std::uint8_t attack = EnvelopeNumberNotSet;
    std::uint8_t decay = EnvelopeNumberNotSet;
    std::uint8_t sustain = EnvelopeNumberNotSet;
    std::uint8_t release = EnvelopeNumberNotSet;

    // Set by the commands that compare. The commands that follow the prefix "if" only run when it is not 0.
    std::uint8_t conditionFlag = 0;

    std::array<std::int16_t, VariableCount> variables;

    // The open calls and loops, the last one on top.
    std::array<ReturnFrame, MaximumCallDepth> returnFrames{};
    std::size_t returnFrameCount = 0;

    std::vector<PlayingNote> notes;

    SequenceTrack()
    {
        variables.fill(StartVariableValue);
    }
};

}
