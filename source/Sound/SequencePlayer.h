#pragma once

#include "Sound/LoadedFiles.h"
#include "Sound/SequenceTrack.h"
#include "Sound/SoundArchiveFile.h"
#include "Sound/VoicePool.h"

#include <array>
#include <cstdint>

namespace Sound
{

// What every sequence that plays shares: the global variables, and the random numbers.
struct SequenceGlobals
{
    // The generator of the original starts with this number.
    static constexpr std::uint32_t StartRandomSeed = 0x12345678;

    std::array<std::int16_t, VariableCount> variables;
    std::uint32_t randomSeed = StartRandomSeed;

    SequenceGlobals()
    {
        variables.fill(StartVariableValue);
    }

    // The next random number, from 0 to 65535. The original takes one in every sound frame, also when no sequence asks for it.
    std::uint32_t NextRandomNumber();
};

// Plays one sequence: it runs the commands of the tracks tick by tick, starts a voice for every note, and keeps the voices
// up to date with the settings of their track. Every sound frame of 3 milliseconds the owner calls Update.
//
// The time of a sequence is counted in ticks, 48 for every beat. How many ticks a sound frame has depends on the tempo, the
// beats per minute, which a command can change. Notes start and stop at the ticks, so music is timed in steps of one frame.
class SequencePlayer
{
public:
    // The player keeps references to everything it is given, so they must outlive it.
    SequencePlayer(const SequenceFile& sequenceFile, const LoadedBank& loadedBank, const SequenceSettings& sequenceSettings, float soundVolume,
        VoicePool& voicePool, SequenceGlobals& sharedGlobals);

    // Moves the sequence on by one sound frame.
    void Update();

    // Lets go of all the notes and ends all the tracks. The voices fade out by their envelopes.
    void Stop();

    // The volume of the whole sequence, from 0 to 1. The notes take it from the next sound frame on.
    void SetVolume(float sequenceVolume);

    // Added to the pan of every note (-1 left, 1 right), and multiplying the pitch of every note. The notes take them from the next
    // sound frame on.
    void SetPan(float sequencePan);
    void SetPitch(float sequencePitch);

    // A held sequence stands still: its ticks do not move on, and the voices of its notes are held where they are.
    void SetHeld(bool held);

    // True when no track runs any more. The voices of the notes can still be sounding.
    bool HasEnded() const;

    // The number of sounding notes.
    std::size_t NoteCount() const;

    // The number of times something went wrong: a track was ended because it ran a command that is not supported, ran out of
    // commands, or ran too long without waiting; or a note had no sound in the bank for its instrument, key and velocity.
    std::size_t ErrorCount() const;

private:
    enum class NumberSource
    {
        Written,
        Random,
        Variable,
    };

    enum class NumberWidth
    {
        Byte,
        Short,
        Variable,
    };

    void RunTick();
    void TickNotes(SequenceTrack& track);
    bool TickTrack(SequenceTrack& track);
    bool RunCommand(SequenceTrack& track);
    void EndTrack(SequenceTrack& track);
    void FailTrack(SequenceTrack& track);

    std::uint8_t ReadByte(SequenceTrack& track);
    std::int16_t ReadShort(SequenceTrack& track);
    std::uint32_t ReadThreeBytes(SequenceTrack& track);
    std::int32_t ReadNumber(SequenceTrack& track, NumberSource source, NumberWidth width);
    std::int16_t* FindVariable(SequenceTrack& track, std::size_t variableNumber);

    bool RunNote(SequenceTrack& track, int key, bool shouldRun, NumberSource source);
    bool RunControlCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source);
    bool RunByteCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source);
    bool RunShortCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source);
    bool RunExtendedCommand(SequenceTrack& track, bool shouldRun, NumberSource source);
    void ChangeVariable(std::uint8_t command, std::int16_t& variable, std::int32_t number);
    void OpenTrack(std::size_t trackNumber, std::size_t position, const SequenceTrack& openingTrack);
    void ReturnFromCall(SequenceTrack& track);
    void StartLoop(SequenceTrack& track, std::uint8_t roundCount);
    void EndLoop(SequenceTrack& track);
    void StartPortamento(SequenceTrack& track, std::uint8_t startKey);
    void SetTie(SequenceTrack& track, bool isTied);
    void MuteTrack(SequenceTrack& track, std::uint8_t mode);

    // How far the modulation of a note was from the middle before the sound frame and is after it.
    struct ModulationValues
    {
        float beforeTheFrame;
        float afterTheFrame;
    };

    void StartNote(SequenceTrack& track, int key, int velocity, int length);
    PlayingNote* StartNewNote(SequenceTrack& track, int key, int velocity, int length);
    PlayingNote* FindNoteToTie(SequenceTrack& track);
    void TieNote(SequenceTrack& track, PlayingNote& note, int key, int velocity);
    void ChangeEnvelopeOfNote(const SequenceTrack& track, const PlayingNote& note);
    void UpdateNotes(SequenceTrack& track);
    void ReleaseAndForgetNotes(SequenceTrack& track);
    void StopAndForgetNotes(SequenceTrack& track);
    VoiceSettings SettingsOfNote(const SequenceTrack& track, const PlayingNote& note, const ModulationValues& modulation) const;

    // A tick is 1/48 of a beat. A frame of 3 milliseconds adds the tempo (the beats per minute) to the progress towards the next tick,
    // and a tick is complete at 416. At 120 beats per minute that is 3.47 frames, 10.4 milliseconds, which is 1/48 of 500 milliseconds.
    static constexpr int TickProgressForOneTick = 416;
    static constexpr int StartTempo = 120;

    // The loudness of all the tracks together, from 0 to 127. A command can change it.
    static constexpr std::uint8_t StartMainVolume = 127;

    const SequenceFile& sequence;
    const LoadedBank& bank;
    SequenceSettings settings;
    float volume;
    float pan = 0.0f;
    float pitch = 1.0f;
    VoicePool& voices;
    SequenceGlobals& globals;

    std::array<SequenceTrack, MaximumTrackCount> tracks;
    std::array<std::int16_t, VariableCount> variables;

    bool isHeld = false;
    int tempo = StartTempo;
    std::uint8_t mainVolume = StartMainVolume;

    // The progress starts complete, so that the first commands run in the first frame.
    int tickProgress = TickProgressForOneTick;
    bool dataRanOut = false;
    std::size_t errorCount = 0;
};

}
