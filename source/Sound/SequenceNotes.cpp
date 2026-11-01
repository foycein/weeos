#include "Sound/SequencePlayer.h"

#include <algorithm>
#include <cmath>

// How the notes of a track are started, kept up to date and let go: the part of the sequence player that makes the voices.

namespace Sound
{

namespace
{

constexpr float HighestNumber = 127.0f;
constexpr float MiddleNumber = 64.0f;
constexpr float NumbersFromMiddleToSide = 63.0f;
constexpr float BendNumbersForTheRange = 128.0f;
constexpr double SemitonesPerOctave = 12.0;

float Square(float number)
{
    return number * number;
}

std::uint8_t EnvelopeNumber(std::uint8_t trackNumber, std::uint8_t instrumentNumber)
{
    return trackNumber == EnvelopeNumberNotSet ? instrumentNumber : trackNumber;
}

// The volume modulation makes a note louder and softer by 6 decibels for each step of its wave. The original looks the decibels up
// in tenths in its loudness table, which goes from -90.4 to 6 decibels (0x8150f31c).
float LoudnessOfVolumeModulation(float modulationValue)
{
    constexpr float DecibelsPerStep = 6.0f;
    constexpr float HighestDecibels = 6.0f;
    constexpr float LowestDecibels = -90.4f;
    constexpr float TableStepsPerDecibel = 10.0f;
    constexpr float DecibelsPerFactorOfTen = 20.0f;

    float decibels = std::clamp(DecibelsPerStep * modulationValue, LowestDecibels, HighestDecibels);
    float wholeSteps = std::trunc(decibels * TableStepsPerDecibel);
    return std::pow(10.0f, wholeSteps / TableStepsPerDecibel / DecibelsPerFactorOfTen);
}

// A note slides to its key when the track has a sweep, or when portamento is on and the key is not that of the note before. Without a
// portamento time the slide takes as long as the note and is counted in ticks. With one it takes the time squared times the
// semitones in steps of 5 milliseconds for every 32, and is counted in milliseconds (0x81505e88).
PitchSweep SweepOfNewNote(const SequenceTrack& track, int key, int length)
{
    constexpr int BitsToDropFromTheSweepTime = 5;
    constexpr int MillisecondsPerSweepStep = 5;

    float semitones = track.sweepSemitones;
    if (track.isPortamentoOn)
        semitones += float(track.portamentoKey - key);

    PitchSweep sweep;
    sweep.semitones = semitones;
    if (track.portamentoTime == 0)
    {
        sweep.length = length;
        return sweep;
    }

    double timeSquared = double(track.portamentoTime) * double(track.portamentoTime);
    int sweepTime = static_cast<int>(timeSquared * double(std::abs(semitones)));
    sweep.length = (sweepTime >> BitsToDropFromTheSweepTime) * MillisecondsPerSweepStep;
    sweep.countsMilliseconds = true;
    return sweep;
}

}

// What a note needs from its track and from the sequence to be heard: loudness, place, pitch, filter.
VoiceSettings SequencePlayer::SettingsOfNote(const SequenceTrack& track, const PlayingNote& note, const ModulationValues& modulation) const
{
    float trackLoudness = float(track.volume) / HighestNumber;
    float expressionLoudness = float(track.expression) / HighestNumber;
    float mainLoudness = float(mainVolume) / HighestNumber;
    float bentSemitones = float(track.bend) / BendNumbersForTheRange * float(track.bendRange);

    VoiceSettings voiceSettings;
    voiceSettings.volume = volume * Square(trackLoudness) * Square(expressionLoudness) * Square(mainLoudness) * note.startVolume;
    voiceSettings.pan = note.startPan + track.pan;
    double semitones = double(note.key - note.note.originalKey) + bentSemitones + note.sweep.SemitonesLeft();

    // The pitch and the pan take the wave as it was before the frame, the volume as it is after it.
    switch (track.modulation.target)
    {
    case ModulationTarget::Pitch: semitones += modulation.beforeTheFrame; break;
    case ModulationTarget::Volume: voiceSettings.volume *= LoudnessOfVolumeModulation(modulation.afterTheFrame); break;
    case ModulationTarget::Pan: voiceSettings.pan += modulation.beforeTheFrame; break;
    case ModulationTarget::None: break;
    }

    voiceSettings.surroundPan = track.surroundPan;
    voiceSettings.mainSend = float(track.mainSend) / HighestNumber;
    voiceSettings.effectSend = float(track.effectSendA) / HighestNumber;
    voiceSettings.pitch = static_cast<float>(std::exp2(semitones / SemitonesPerOctave)) * note.note.tune;
    voiceSettings.lowPass = LowPassValueFromCutoffNumber(track.lowPassCutoff);
    return voiceSettings;
}

void SequencePlayer::StartNote(SequenceTrack& track, int key, int velocity, int length)
{
    if (track.isMuted)
        return;

    PlayingNote* note = track.isTied ? FindNoteToTie(track) : nullptr;
    if (note != nullptr)
        TieNote(track, *note, key, velocity);
    else
        note = StartNewNote(track, key, velocity, length);

    if (note == nullptr)
        return;

    note->sweep = SweepOfNewNote(track, key, length);
    track.portamentoKey = key;
}

// Gives the note started, or nothing when there is no sound for it or no voice.
PlayingNote* SequencePlayer::StartNewNote(SequenceTrack& track, int key, int velocity, int length)
{
    std::optional<NoteParameters> foundNote = bank.file.FindNote(track.program, key, velocity);
    if (!foundNote)
    {
        ++errorCount;
        return nullptr;
    }

    const NoteParameters& note = *foundNote;
    std::uint8_t attack = EnvelopeNumber(track.attack, note.attack);
    std::uint8_t decay = EnvelopeNumber(track.decay, note.decay);
    std::uint8_t sustain = EnvelopeNumber(track.sustain, note.sustain);
    std::uint8_t release = EnvelopeNumber(track.release, note.release);
    Envelope envelope(attack, decay, sustain, release);

    // A tied note is held until the tie ends.
    PlayingNote playingNote;
    playingNote.ticksLeft = track.isTied ? -1 : length;
    playingNote.key = key;
    playingNote.note = note;
    playingNote.startVolume = Square(float(velocity) / HighestNumber) * float(note.volume) / HighestNumber;
    playingNote.startPan = (float(note.pan) - MiddleNumber) / NumbersFromMiddleToSide + track.initialPan;

    int priority = settings.channelPriority + track.priority;
    VoiceSettings voiceSettings = SettingsOfNote(track, playingNote, ModulationValues{0.0f, 0.0f});
    std::optional<VoiceHandle> voice = voices.Start(bank.waves[note.waveIndex], envelope, priority, voiceSettings);
    if (!voice)
        return nullptr;

    playingNote.voice = *voice;
    track.notes.push_back(playingNote);
    return &track.notes.back();
}

// The newest note whose voice still sounds.
PlayingNote* SequencePlayer::FindNoteToTie(SequenceTrack& track)
{
    for (auto note = track.notes.rbegin(); note != track.notes.rend(); ++note)
    {
        if (voices.Find(note->voice) != nullptr)
            return &*note;
    }
    return nullptr;
}

// A tied note takes over the voice of the note before it. Only its key and the loudness of its velocity change, and the envelope
// numbers that the track sets. The original leaves the volume of the instrument out here (0x81505e88).
void SequencePlayer::TieNote(SequenceTrack& track, PlayingNote& note, int key, int velocity)
{
    note.key = key;
    note.startVolume = Square(float(velocity) / HighestNumber);
    ChangeEnvelopeOfNote(track, note);
}

void SequencePlayer::ChangeEnvelopeOfNote(const SequenceTrack& track, const PlayingNote& note)
{
    Voice* voice = voices.Find(note.voice);
    if (voice != nullptr)
        voice->ChangeEnvelopeNumbers(track.attack, track.decay, track.sustain, track.release);
}

// The notes with a length count down, and let go of their key when the length is over. A slide that is counted in ticks moves on.
void SequencePlayer::TickNotes(SequenceTrack& track)
{
    for (PlayingNote& note : track.notes)
    {
        if (!note.sweep.countsMilliseconds)
            note.sweep.Advance(1);

        if (note.ticksLeft <= 0)
            continue;

        --note.ticksLeft;
        if (note.ticksLeft > 0)
            continue;

        Voice* voice = voices.Find(note.voice);
        if (voice != nullptr)
            voice->Release();
    }
}

// Gives every voice the settings of the track, and forgets the notes whose voice has ended. The wave of the modulation and a slide
// that is counted in milliseconds move on by the frame.
void SequencePlayer::UpdateNotes(SequenceTrack& track)
{
    std::erase_if(track.notes, [&](const PlayingNote& note) { return voices.Find(note.voice) == nullptr; });
    for (PlayingNote& note : track.notes)
    {
        ModulationValues modulationValues;
        modulationValues.beforeTheFrame = note.modulation.Value(track.modulation);
        note.modulation.Advance(track.modulation, SoundFrameMilliseconds);
        modulationValues.afterTheFrame = note.modulation.Value(track.modulation);

        if (note.sweep.countsMilliseconds)
            note.sweep.Advance(SoundFrameMilliseconds);

        voices.Find(note.voice)->SetSettings(SettingsOfNote(track, note, modulationValues));
    }
}

// The voices fade out by their envelopes. The track no longer keeps them up to date.
void SequencePlayer::ReleaseAndForgetNotes(SequenceTrack& track)
{
    for (PlayingNote& note : track.notes)
    {
        Voice* voice = voices.Find(note.voice);
        if (voice != nullptr)
            voice->Release();
    }
    track.notes.clear();
}

void SequencePlayer::StopAndForgetNotes(SequenceTrack& track)
{
    for (PlayingNote& note : track.notes)
    {
        Voice* voice = voices.Find(note.voice);
        if (voice != nullptr)
            voice->Stop();
    }
    track.notes.clear();
}

}
