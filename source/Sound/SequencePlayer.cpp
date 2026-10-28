#include "Sound/SequencePlayer.h"

#include <algorithm>
#include <cmath>

namespace Sound
{

namespace
{

// A tick is 1/48 of a beat. A frame of 3 milliseconds adds the tempo (the beats per minute) to the progress towards the next tick,
// and a tick is complete at 416. At 120 beats per minute that is 3.47 frames, 10.4 milliseconds, which is 1/48 of 500 milliseconds.
// The progress starts complete, so that the first commands run in the first frame.
constexpr int TickProgressForOneTick = 416;
constexpr int StartTempo = 120;

// A track that runs more commands than this without waiting is stopped. No real sequence does, and the audio thread must not hang.
constexpr int MostCommandsInOneTick = 10000;

constexpr float HighestNumber = 127.0f;
constexpr float BendRangeInSemitones = 2.0f;
constexpr float BendNumbersForTheRange = 128.0f;
constexpr double SemitonesPerOctave = 12.0;

// The commands. A command below 0x80 is a note, and its number is the key.
constexpr std::uint8_t FirstControlCommand = 0x80;
constexpr std::uint8_t WaitCommand = 0x80;
constexpr std::uint8_t ProgramCommand = 0x81;
constexpr std::uint8_t OpenTrackCommand = 0x88;
constexpr std::uint8_t JumpCommand = 0x89;
constexpr std::uint8_t CallCommand = 0x8A;
constexpr std::uint8_t PanCommand = 0xC0;
constexpr std::uint8_t VolumeCommand = 0xC1;
constexpr std::uint8_t TransposeCommand = 0xC3;
constexpr std::uint8_t BendCommand = 0xC4;
constexpr std::uint8_t NoteWaitCommand = 0xC7;
constexpr std::uint8_t AttackCommand = 0xD0;
constexpr std::uint8_t DecayCommand = 0xD1;
constexpr std::uint8_t SustainCommand = 0xD2;
constexpr std::uint8_t ReleaseCommand = 0xD3;
constexpr std::uint8_t SurroundPanCommand = 0xD7;
constexpr std::uint8_t EffectSendACommand = 0xD9;
constexpr std::uint8_t EffectSendBCommand = 0xDA;
constexpr std::uint8_t TempoCommand = 0xE1;
constexpr std::uint8_t ExtendedCommand = 0xF0;
constexpr std::uint8_t ReturnCommand = 0xFD;
constexpr std::uint8_t AllocateTracksCommand = 0xFE;
constexpr std::uint8_t EndCommand = 0xFF;

// A prefix changes where the arguments of the next command come from, or whether it runs at all.
constexpr std::uint8_t RandomPrefix = 0xA0;
constexpr std::uint8_t VariablePrefix = 0xA1;
constexpr std::uint8_t IfPrefix = 0xA2;

// The extended commands work on variables. The ones from 0x80 change a variable, the ones from 0x90 compare it.
constexpr std::uint8_t SetVariableCommand = 0x80;
constexpr std::uint8_t AddVariableCommand = 0x81;
constexpr std::uint8_t SubtractVariableCommand = 0x82;
constexpr std::uint8_t MultiplyVariableCommand = 0x83;
constexpr std::uint8_t DivideVariableCommand = 0x84;
constexpr std::uint8_t ShiftVariableCommand = 0x85;
constexpr std::uint8_t RandomVariableCommand = 0x86;
constexpr std::uint8_t AndVariableCommand = 0x87;
constexpr std::uint8_t OrVariableCommand = 0x88;
constexpr std::uint8_t XorVariableCommand = 0x89;
constexpr std::uint8_t NotVariableCommand = 0x8A;
constexpr std::uint8_t RemainderVariableCommand = 0x8B;
constexpr std::uint8_t IsEqualCommand = 0x90;
constexpr std::uint8_t IsGreaterOrEqualCommand = 0x91;
constexpr std::uint8_t IsGreaterCommand = 0x92;
constexpr std::uint8_t IsLessOrEqualCommand = 0x93;
constexpr std::uint8_t IsLessCommand = 0x94;
constexpr std::uint8_t IsNotEqualCommand = 0x95;

// Variables 0 to 15 belong to the sequence, 16 to 31 to all sequences, and 32 to 47 to the track.
constexpr std::size_t FirstGlobalVariable = 16;
constexpr std::size_t FirstTrackVariable = 32;
constexpr std::size_t FirstUnusedVariable = 48;

constexpr std::uint32_t RandomMultiplier = 0x19660D;
constexpr std::uint32_t RandomIncrement = 0x3C6EF35F;
constexpr int RandomBitsToDrop = 16;
constexpr int ShiftLimit = 31;

float Square(float number)
{
    return number * number;
}

std::uint32_t NextRandomNumber(SequenceGlobals& globals)
{
    globals.randomSeed = globals.randomSeed * RandomMultiplier + RandomIncrement;
    return globals.randomSeed >> RandomBitsToDrop;
}

// A number between the lowest and the highest, both included.
std::int32_t RandomNumberBetween(SequenceGlobals& globals, std::int32_t lowest, std::int32_t highest)
{
    std::int64_t choices = std::int64_t(highest) - lowest + 1;
    return static_cast<std::int32_t>(((std::int64_t(NextRandomNumber(globals)) * choices) >> RandomBitsToDrop) + lowest);
}

// What a note needs from its track to be heard: loudness, place, pitch.
VoiceSettings SettingsOfNote(const SequenceTrack& track, const PlayingNote& note, float soundVolume)
{
    float trackLoudness = float(track.volume) / HighestNumber;
    float bentSemitones = float(track.bend) / BendNumbersForTheRange * BendRangeInSemitones;
    double semitones = double(note.key - note.note.originalKey) + bentSemitones;

    VoiceSettings settings;
    settings.volume = soundVolume * Square(trackLoudness) * note.startVolume;
    settings.pan = note.startPan + track.pan;
    settings.surroundPan = track.surroundPan;
    settings.mainSend = float(track.mainSend) / HighestNumber;
    settings.pitch = static_cast<float>(std::exp2(semitones / SemitonesPerOctave)) * note.note.tune;
    return settings;
}

// The result of one of the commands that compare a variable with a number.
bool CompareVariable(std::uint8_t command, std::int16_t variable, std::int32_t number)
{
    switch (command)
    {
    case IsEqualCommand: return variable == number;
    case IsGreaterOrEqualCommand: return variable >= number;
    case IsGreaterCommand: return variable > number;
    case IsLessOrEqualCommand: return variable <= number;
    case IsLessCommand: return variable < number;
    default: return variable != number;
    }
}

std::uint8_t EnvelopeNumber(std::uint8_t trackNumber, std::uint8_t instrumentNumber)
{
    return trackNumber == EnvelopeNumberNotSet ? instrumentNumber : trackNumber;
}

}

SequencePlayer::SequencePlayer(const SequenceFile& sequenceFile, const LoadedBank& loadedBank, const SequenceSettings& sequenceSettings,
    float soundVolume, VoicePool& voicePool, SequenceGlobals& sharedGlobals)
    : sequence(sequenceFile)
    , bank(loadedBank)
    , settings(sequenceSettings)
    , volume(soundVolume)
    , voices(voicePool)
    , globals(sharedGlobals)
    , tempo(StartTempo)
    , tickProgress(TickProgressForOneTick)
{
    variables.fill(-1);

    for (std::size_t trackNumber = 0; trackNumber < tracks.size(); ++trackNumber)
        tracks[trackNumber].isAllocated = (settings.trackMask >> trackNumber & 1) != 0;

    if (tracks[0].isAllocated)
    {
        tracks[0].position = settings.startOffset;
        tracks[0].isRunning = true;
    }
}

void SequencePlayer::Update()
{
    int tickCount = 0;
    while (tickProgress >= TickProgressForOneTick)
    {
        ++tickCount;
        tickProgress -= TickProgressForOneTick;
    }
    tickProgress += tempo;

    for (int tick = 0; tick < tickCount; ++tick)
        RunTick();

    for (SequenceTrack& track : tracks)
        UpdateNotes(track);
}

void SequencePlayer::Stop()
{
    for (SequenceTrack& track : tracks)
    {
        if (track.isRunning)
            EndTrack(track);
    }
}

bool SequencePlayer::HasEnded() const
{
    return std::none_of(tracks.begin(), tracks.end(), [](const SequenceTrack& track) { return track.isRunning; });
}

std::size_t SequencePlayer::NoteCount() const
{
    std::size_t noteCount = 0;
    for (const SequenceTrack& track : tracks)
        noteCount += track.notes.size();

    return noteCount;
}

std::size_t SequencePlayer::ErrorCount() const
{
    return errorCount;
}

// ---- Ticks

void SequencePlayer::RunTick()
{
    for (SequenceTrack& track : tracks)
    {
        if (!track.isRunning)
            continue;

        TickNotes(track);
        if (TickTrack(track))
            EndTrack(track);
    }
}

// The notes with a length count down, and let go of their key when the length is over.
void SequencePlayer::TickNotes(SequenceTrack& track)
{
    for (PlayingNote& note : track.notes)
    {
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

// Waits, or runs commands until one of them makes the track wait. True when the track has ended.
bool SequencePlayer::TickTrack(SequenceTrack& track)
{
    if (track.isWaitingForNotes)
    {
        std::erase_if(track.notes, [&](const PlayingNote& note) { return voices.Find(note.voice) == nullptr; });
        if (!track.notes.empty())
            return false;

        track.isWaitingForNotes = false;
    }

    if (track.waitTicks > 0)
    {
        --track.waitTicks;
        if (track.waitTicks > 0)
            return false;
    }

    int commandCount = 0;
    while (track.waitTicks == 0 && !track.isWaitingForNotes)
    {
        if (++commandCount > MostCommandsInOneTick)
        {
            FailTrack(track);
            return true;
        }

        if (RunCommand(track))
            return true;
    }
    return false;
}

void SequencePlayer::EndTrack(SequenceTrack& track)
{
    UpdateNotes(track);
    for (PlayingNote& note : track.notes)
    {
        Voice* voice = voices.Find(note.voice);
        if (voice != nullptr)
            voice->Release();
    }

    track.notes.clear();
    track.isRunning = false;
    track.isWaitingForNotes = false;
}

void SequencePlayer::FailTrack(SequenceTrack& track)
{
    ++errorCount;
    track.waitTicks = 0;
}

// ---- Reading commands

std::uint8_t SequencePlayer::ReadByte(SequenceTrack& track)
{
    if (track.position >= sequence.commands.size())
    {
        dataRanOut = true;
        return 0;
    }

    return sequence.commands[track.position++];
}

std::int16_t SequencePlayer::ReadShort(SequenceTrack& track)
{
    std::uint16_t high = ReadByte(track);
    std::uint16_t low = ReadByte(track);
    return static_cast<std::int16_t>(high << 8 | low);
}

std::uint32_t SequencePlayer::ReadThreeBytes(SequenceTrack& track)
{
    std::uint32_t first = ReadByte(track);
    std::uint32_t second = ReadByte(track);
    std::uint32_t third = ReadByte(track);
    return (first << 16) | (second << 8) | third;
}

std::int16_t* SequencePlayer::FindVariable(SequenceTrack& track, std::size_t variableNumber)
{
    if (variableNumber < FirstGlobalVariable)
        return &variables[variableNumber];

    if (variableNumber < FirstTrackVariable)
        return &globals.variables[variableNumber - FirstGlobalVariable];

    if (variableNumber < FirstUnusedVariable)
        return &track.variables[variableNumber - FirstTrackVariable];

    return nullptr;
}

// A number of a command is written in the commands, or it is chosen by chance between two written numbers, or it is the value of a variable.
// The written number has a width: a byte, two bytes, or as many bytes as it needs (7 bits in each, the last byte has the top bit clear).
std::int32_t SequencePlayer::ReadNumber(SequenceTrack& track, NumberSource source, NumberWidth width)
{
    constexpr std::uint8_t MoreBytesFollowBit = 0x80;
    constexpr int BitsInOneByteOfALongNumber = 7;

    if (source == NumberSource::Random)
    {
        std::int16_t lowest = ReadShort(track);
        std::int16_t highest = ReadShort(track);
        return RandomNumberBetween(globals, lowest, highest);
    }

    if (source == NumberSource::Variable)
    {
        std::int16_t* variable = FindVariable(track, ReadByte(track));
        return variable == nullptr ? 0 : *variable;
    }

    if (width == NumberWidth::Byte)
        return ReadByte(track);

    if (width == NumberWidth::Short)
        return ReadShort(track);

    std::int32_t number = 0;
    std::uint8_t byte = 0;
    do
    {
        byte = ReadByte(track);
        number = (number << BitsInOneByteOfALongNumber) | (byte & ~MoreBytesFollowBit);
    } while ((byte & MoreBytesFollowBit) != 0 && !dataRanOut);
    return number;
}

// ---- Running commands

// Runs the next command. True when the track has ended.
bool SequencePlayer::RunCommand(SequenceTrack& track)
{
    dataRanOut = false;

    std::uint8_t command = ReadByte(track);
    bool shouldRun = true;
    if (command == IfPrefix)
    {
        shouldRun = track.conditionFlag != 0;
        command = ReadByte(track);
    }

    NumberSource source = NumberSource::Written;
    if (command == RandomPrefix)
    {
        source = NumberSource::Random;
        command = ReadByte(track);
    }
    else if (command == VariablePrefix)
    {
        source = NumberSource::Variable;
        command = ReadByte(track);
    }

    bool trackEnded = command < FirstControlCommand ? RunNote(track, command, shouldRun, source) : RunControlCommand(track, command, shouldRun, source);
    if (dataRanOut)
    {
        FailTrack(track);
        return true;
    }
    return trackEnded;
}

// A note has the velocity and the length. The length is a number of ticks, and 0 means "until the wave is over".
bool SequencePlayer::RunNote(SequenceTrack& track, int key, bool shouldRun, NumberSource source)
{
    constexpr int HighestKey = 127;

    int velocity = ReadByte(track);
    int length = std::max(0, ReadNumber(track, source, NumberWidth::Variable));
    if (!shouldRun)
        return false;

    StartNote(track, std::clamp(key + track.transpose, 0, HighestKey), velocity, length > 0 ? length : -1);

    if (track.noteWait)
    {
        track.waitTicks = length;
        track.isWaitingForNotes = length == 0;
    }
    return false;
}

bool SequencePlayer::RunControlCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source)
{
    switch (command)
    {
    case WaitCommand:
    {
        int ticks = std::max(0, ReadNumber(track, source, NumberWidth::Variable));
        if (shouldRun)
            track.waitTicks = ticks;
        return false;
    }
    case ProgramCommand:
    {
        std::int32_t program = ReadNumber(track, source, NumberWidth::Variable);
        if (shouldRun)
            track.program = static_cast<std::uint16_t>(program);
        return false;
    }
    case OpenTrackCommand:
    {
        std::size_t trackNumber = ReadByte(track);
        std::size_t position = ReadThreeBytes(track);
        if (shouldRun)
            OpenTrack(trackNumber, position, track);
        return false;
    }
    case JumpCommand:
    {
        std::size_t position = ReadThreeBytes(track);
        if (shouldRun)
            track.position = position;
        return false;
    }
    case CallCommand:
    {
        std::size_t position = ReadThreeBytes(track);
        if (shouldRun && track.returnPointCount < MaximumCallDepth)
        {
            track.returnPoints[track.returnPointCount++] = {track.position, 0};
            track.position = position;
        }
        return false;
    }
    case ReturnCommand:
        if (shouldRun)
            ReturnFromCall(track);
        return false;
    case AllocateTracksCommand:
        ReadByte(track);
        ReadByte(track);
        return false;
    case EndCommand:
        return true;
    case ExtendedCommand:
        return RunExtendedCommand(track, shouldRun, source);
    case TempoCommand:
    {
        std::int32_t newTempo = ReadNumber(track, source, NumberWidth::Short);
        if (shouldRun)
            tempo = std::max(0, newTempo);
        return false;
    }
    default:
        return RunSettingCommand(track, command, shouldRun, source);
    }
}

// The commands that set one value of the track: the number is a byte.
bool SequencePlayer::RunSettingCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source)
{
    bool isSettingCommand = command == PanCommand || command == VolumeCommand || command == TransposeCommand || command == BendCommand ||
        command == NoteWaitCommand || (command >= AttackCommand && command <= ReleaseCommand) || command == SurroundPanCommand ||
        command == EffectSendACommand || command == EffectSendBCommand;
    if (!isSettingCommand)
    {
        FailTrack(track);
        return true;
    }

    std::int32_t number = ReadNumber(track, source, NumberWidth::Byte);
    if (!shouldRun)
        return false;

    std::uint8_t unsignedNumber = static_cast<std::uint8_t>(number);
    int signedNumber = static_cast<std::int8_t>(number);
    switch (command)
    {
    case PanCommand: track.pan = PanFromNumber(unsignedNumber); break;
    case VolumeCommand: track.volume = std::min<std::uint8_t>(unsignedNumber, 127); break;
    case TransposeCommand: track.transpose = signedNumber; break;
    case BendCommand: track.bend = signedNumber; break;
    case NoteWaitCommand: track.noteWait = unsignedNumber != 0; break;
    case AttackCommand: track.attack = unsignedNumber; break;
    case DecayCommand: track.decay = unsignedNumber; break;
    case SustainCommand: track.sustain = unsignedNumber; break;
    case ReleaseCommand: track.release = unsignedNumber; break;
    case SurroundPanCommand: track.surroundPan = SurroundPanFromNumber(unsignedNumber); break;
    default: break;
    }
    return false;
}

bool SequencePlayer::RunExtendedCommand(SequenceTrack& track, bool shouldRun, NumberSource source)
{
    std::uint8_t command = ReadByte(track);
    bool changesVariable = command >= SetVariableCommand && command <= RemainderVariableCommand;
    bool comparesVariable = command >= IsEqualCommand && command <= IsNotEqualCommand;
    if (!changesVariable && !comparesVariable)
    {
        FailTrack(track);
        return true;
    }

    std::size_t variableNumber = ReadByte(track);
    std::int32_t number = ReadNumber(track, source, NumberWidth::Short);
    std::int16_t* variable = FindVariable(track, variableNumber);
    if (!shouldRun || variable == nullptr)
        return false;

    if (changesVariable)
        ChangeVariable(command, *variable, number);
    else
        track.conditionFlag = CompareVariable(command, *variable, number) ? 1 : 0;

    return false;
}

void SequencePlayer::ChangeVariable(std::uint8_t command, std::int16_t& variable, std::int32_t number)
{
    std::int32_t current = variable;
    std::int32_t result = current;

    switch (command)
    {
    case SetVariableCommand: result = number; break;
    case AddVariableCommand: result = current + number; break;
    case SubtractVariableCommand: result = current - number; break;
    case MultiplyVariableCommand: result = current * number; break;
    case DivideVariableCommand: result = number != 0 ? current / number : current; break;
    case ShiftVariableCommand:
        if (number < 0)
            result = current >> std::min(-number, ShiftLimit);
        else
            result = current << std::min(number, ShiftLimit);
        break;
    case RandomVariableCommand:
    {
        std::int32_t size = std::abs(number);
        std::int32_t random = static_cast<std::int32_t>((std::int64_t(NextRandomNumber(globals)) * (size + 1)) >> RandomBitsToDrop);
        result = number < 0 ? -random : random;
        break;
    }
    case AndVariableCommand: result = current & number; break;
    case OrVariableCommand: result = current | number; break;
    case XorVariableCommand: result = current ^ number; break;
    case NotVariableCommand: result = ~number; break;
    case RemainderVariableCommand: result = number != 0 ? current % number : current; break;
    default: break;
    }

    variable = static_cast<std::int16_t>(result);
}

// Starts another track of the sequence. A track that exists stops what it does and starts at the place given.
void SequencePlayer::OpenTrack(std::size_t trackNumber, std::size_t position, const SequenceTrack& openingTrack)
{
    if (trackNumber >= tracks.size() || !tracks[trackNumber].isAllocated || &tracks[trackNumber] == &openingTrack)
        return;

    SequenceTrack& opened = tracks[trackNumber];
    if (opened.isRunning)
        EndTrack(opened);

    opened.position = position;
    opened.waitTicks = 0;
    opened.isRunning = true;
}

void SequencePlayer::ReturnFromCall(SequenceTrack& track)
{
    if (track.returnPointCount == 0)
        return;

    --track.returnPointCount;
    track.position = track.returnPoints[track.returnPointCount].position;
}

// ---- Notes

void SequencePlayer::StartNote(SequenceTrack& track, int key, int velocity, int length)
{
    std::optional<NoteParameters> foundNote = bank.file.FindNote(track.program, key, velocity);
    if (!foundNote)
        return;

    const NoteParameters& note = *foundNote;
    Envelope envelope(EnvelopeNumber(track.attack, note.attack), EnvelopeNumber(track.decay, note.decay), EnvelopeNumber(track.sustain, note.sustain),
        EnvelopeNumber(track.release, note.release));

    constexpr float NumbersFromMiddleToSide = 63.0f;
    constexpr float MiddleNumber = 64.0f;
    PlayingNote playingNote;
    playingNote.ticksLeft = length;
    playingNote.key = key;
    playingNote.note = note;
    playingNote.startVolume = Square(float(velocity) / HighestNumber) * float(note.volume) / HighestNumber;
    playingNote.startPan = (float(note.pan) - MiddleNumber) / NumbersFromMiddleToSide;

    int priority = settings.channelPriority + track.priority;
    std::optional<VoiceHandle> voice = voices.Start(bank.waves[note.waveIndex], envelope, priority, SettingsOfNote(track, playingNote, volume));
    if (!voice)
        return;

    playingNote.voice = *voice;
    track.notes.push_back(playingNote);
}

// Gives every voice of the track the settings of the track, and forgets the notes whose voice has ended.
void SequencePlayer::UpdateNotes(SequenceTrack& track)
{
    std::erase_if(track.notes, [&](const PlayingNote& note) { return voices.Find(note.voice) == nullptr; });
    for (const PlayingNote& note : track.notes)
        voices.Find(note.voice)->SetSettings(SettingsOfNote(track, note, volume));
}

}
