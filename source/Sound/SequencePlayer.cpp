#include "Sound/SequencePlayer.h"

#include <algorithm>
#include <cmath>

namespace Sound
{

namespace
{

// A track that runs more commands than this without waiting is stopped. No real sequence does, and the audio thread must not hang.
constexpr int MostCommandsInOneTick = 10000;

constexpr int BitsInOneByte = 8;

constexpr std::uint8_t HighestSetting = 127;

// The commands. A command below 0x80 is a note, and its number is the key.
constexpr std::uint8_t FirstControlCommand = 0x80;
constexpr std::uint8_t WaitCommand = 0x80;
constexpr std::uint8_t ProgramCommand = 0x81;
constexpr std::uint8_t OpenTrackCommand = 0x88;
constexpr std::uint8_t JumpCommand = 0x89;
constexpr std::uint8_t CallCommand = 0x8A;

// The commands from 0xC0 to 0xDF have a byte as their number.
constexpr std::uint8_t FirstByteCommand = 0xC0;
constexpr std::uint8_t LastByteCommand = 0xDF;
constexpr std::uint8_t PanCommand = 0xC0;
constexpr std::uint8_t VolumeCommand = 0xC1;
constexpr std::uint8_t MainVolumeCommand = 0xC2;
constexpr std::uint8_t TransposeCommand = 0xC3;
constexpr std::uint8_t BendCommand = 0xC4;
constexpr std::uint8_t BendRangeCommand = 0xC5;
constexpr std::uint8_t PriorityCommand = 0xC6;
constexpr std::uint8_t NoteWaitCommand = 0xC7;
constexpr std::uint8_t TieCommand = 0xC8;
constexpr std::uint8_t PortamentoCommand = 0xC9;
constexpr std::uint8_t ModulationDepthCommand = 0xCA;
constexpr std::uint8_t ModulationSpeedCommand = 0xCB;
constexpr std::uint8_t ModulationTargetCommand = 0xCC;
constexpr std::uint8_t ModulationRangeCommand = 0xCD;
constexpr std::uint8_t PortamentoSwitchCommand = 0xCE;
constexpr std::uint8_t PortamentoTimeCommand = 0xCF;
constexpr std::uint8_t AttackCommand = 0xD0;
constexpr std::uint8_t DecayCommand = 0xD1;
constexpr std::uint8_t SustainCommand = 0xD2;
constexpr std::uint8_t ReleaseCommand = 0xD3;
constexpr std::uint8_t LoopStartCommand = 0xD4;
constexpr std::uint8_t ExpressionCommand = 0xD5;
constexpr std::uint8_t PrintVariableCommand = 0xD6;
constexpr std::uint8_t SurroundPanCommand = 0xD7;
constexpr std::uint8_t LowPassCutoffCommand = 0xD8;
constexpr std::uint8_t EffectSendACommand = 0xD9;
constexpr std::uint8_t EffectSendBCommand = 0xDA;
constexpr std::uint8_t MainSendCommand = 0xDB;
constexpr std::uint8_t InitialPanCommand = 0xDC;
constexpr std::uint8_t MuteCommand = 0xDD;
constexpr std::uint8_t EffectSendCCommand = 0xDE;
constexpr std::uint8_t DamperCommand = 0xDF;

// The commands from 0xE0 to 0xEF have two bytes as their number.
constexpr std::uint8_t FirstShortCommand = 0xE0;
constexpr std::uint8_t LastShortCommand = 0xEF;
constexpr std::uint8_t ModulationDelayCommand = 0xE0;
constexpr std::uint8_t TempoCommand = 0xE1;
constexpr std::uint8_t SweepPitchCommand = 0xE3;

constexpr std::uint8_t ExtendedCommand = 0xF0;
constexpr std::uint8_t LoopEndCommand = 0xFC;
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

// How much the numbers of the modulation and sweep commands are worth (0x81695328, 0x8169532c, 0x81695330 and 0x81502250).
constexpr float ModulationDepthPerNumber = 1.0f / 128.0f;
constexpr float ModulationSpeedPerNumber = 0.390625f;
constexpr int ModulationDelayMillisecondsPerNumber = 5;
constexpr float SweepSemitonesPerNumber = 1.0f / 32.0f;

// A number between the lowest and the highest, both included.
std::int32_t RandomNumberBetween(SequenceGlobals& globals, std::int32_t lowest, std::int32_t highest)
{
    std::int64_t choices = std::int64_t(highest) - lowest + 1;
    std::int64_t randomNumber = globals.NextRandomNumber();
    std::int64_t chosen = (randomNumber * choices) >> RandomBitsToDrop;
    return static_cast<std::int32_t>(chosen + lowest);
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

}

std::uint32_t SequenceGlobals::NextRandomNumber()
{
    randomSeed = randomSeed * RandomMultiplier + RandomIncrement;
    return randomSeed >> RandomBitsToDrop;
}

SequencePlayer::SequencePlayer(const SequenceFile& sequenceFile, const LoadedBank& loadedBank, const SequenceSettings& sequenceSettings,
    float soundVolume, VoicePool& voicePool, SequenceGlobals& sharedGlobals)
    : sequence(sequenceFile)
    , bank(loadedBank)
    , settings(sequenceSettings)
    , volume(soundVolume)
    , voices(voicePool)
    , globals(sharedGlobals)
{
    variables.fill(StartVariableValue);

    for (std::size_t trackNumber = 0; trackNumber < tracks.size(); ++trackNumber)
    {
        std::uint32_t trackBit = std::uint32_t(1) << trackNumber;
        tracks[trackNumber].isAllocated = (settings.trackMask & trackBit) != 0;
    }

    if (tracks[0].isAllocated)
    {
        tracks[0].position = settings.startOffset;
        tracks[0].isRunning = true;
    }
}

void SequencePlayer::Update()
{
    if (isHeld)
        return;

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

void SequencePlayer::SetVolume(float sequenceVolume)
{
    volume = sequenceVolume;
}

void SequencePlayer::SetHeld(bool held)
{
    isHeld = held;
    for (SequenceTrack& track : tracks)
    {
        for (PlayingNote& note : track.notes)
        {
            Voice* voice = voices.Find(note.voice);
            if (voice != nullptr)
                voice->SetHeld(held);
        }
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
        ++commandCount;
        if (commandCount > MostCommandsInOneTick)
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
    ReleaseAndForgetNotes(track);
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
    return static_cast<std::int16_t>(high << BitsInOneByte | low);
}

std::uint32_t SequencePlayer::ReadThreeBytes(SequenceTrack& track)
{
    std::uint32_t first = ReadByte(track);
    std::uint32_t second = ReadByte(track);
    std::uint32_t third = ReadByte(track);
    return (first << 2 * BitsInOneByte) | (second << BitsInOneByte) | third;
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

// A number of a command is written in the commands, or it is chosen by chance between two written numbers,
// or it is the value of a variable.
// The written number has a width: a byte, two bytes, or as many bytes as it needs (7 bits in each, the last byte has the top bit clear).
std::int32_t SequencePlayer::ReadNumber(SequenceTrack& track, NumberSource source, NumberWidth width)
{
    constexpr std::uint8_t MoreBytesFollowBit = 0x80;
    constexpr std::uint8_t NumberBitsOfAByte = 0x7F;
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
        int numberBits = byte & NumberBitsOfAByte;
        number = (number << BitsInOneByteOfALongNumber) | numberBits;
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

    bool trackEnded = false;
    if (command < FirstControlCommand)
        trackEnded = RunNote(track, command, shouldRun, source);
    else
        trackEnded = RunControlCommand(track, command, shouldRun, source);

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

    int transposedKey = std::clamp(key + track.transpose, 0, HighestKey);
    int noteLength = length > 0 ? length : HeldUntilReleased;
    StartNote(track, transposedKey, velocity, noteLength);

    if (track.noteWait)
    {
        track.waitTicks = length;
        track.isWaitingForNotes = length == 0;
    }
    return false;
}

bool SequencePlayer::RunControlCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source)
{
    if (command >= FirstByteCommand && command <= LastByteCommand)
        return RunByteCommand(track, command, shouldRun, source);

    if (command >= FirstShortCommand && command <= LastShortCommand)
        return RunShortCommand(track, command, shouldRun, source);

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
        if (shouldRun && track.returnFrameCount < MaximumCallDepth)
        {
            track.returnFrames[track.returnFrameCount] = ReturnFrame{track.position, 0};
            ++track.returnFrameCount;
            track.position = position;
        }
        return false;
    }
    case ReturnCommand:
        if (shouldRun)
            ReturnFromCall(track);
        return false;
    case LoopEndCommand:
        if (shouldRun)
            EndLoop(track);
        return false;
    case AllocateTracksCommand:
        ReadByte(track);
        ReadByte(track);
        return false;
    case EndCommand:
        return true;
    case ExtendedCommand:
        return RunExtendedCommand(track, shouldRun, source);
    default:
        FailTrack(track);
        return true;
    }
}

// The commands with a byte as their number set one value of the track, or of the whole sequence. The ones for the effect buses B and C,
// the damper and the printing of a variable do nothing here: no effect hangs on those buses, and the other two make no sound.
bool SequencePlayer::RunByteCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source)
{
    std::int32_t number = ReadNumber(track, source, NumberWidth::Byte);
    if (!shouldRun)
        return false;

    std::uint8_t unsignedNumber = static_cast<std::uint8_t>(number);
    int signedNumber = static_cast<std::int8_t>(number);
    std::uint8_t settingNumber = std::min(unsignedNumber, HighestSetting);
    switch (command)
    {
    case PanCommand: track.pan = PanFromNumber(unsignedNumber); break;
    case VolumeCommand: track.volume = settingNumber; break;
    case MainVolumeCommand: mainVolume = settingNumber; break;
    case TransposeCommand: track.transpose = signedNumber; break;
    case BendCommand: track.bend = signedNumber; break;
    case BendRangeCommand: track.bendRange = unsignedNumber; break;
    case PriorityCommand: track.priority = unsignedNumber; break;
    case NoteWaitCommand: track.noteWait = unsignedNumber != 0; break;
    case TieCommand: SetTie(track, unsignedNumber != 0); break;
    case PortamentoCommand: StartPortamento(track, unsignedNumber); break;
    case PortamentoSwitchCommand: track.isPortamentoOn = unsignedNumber != 0; break;
    case PortamentoTimeCommand: track.portamentoTime = unsignedNumber; break;
    case ModulationDepthCommand: track.modulation.depth = float(unsignedNumber) * ModulationDepthPerNumber; break;
    case ModulationSpeedCommand: track.modulation.speed = float(unsignedNumber) * ModulationSpeedPerNumber; break;
    case ModulationTargetCommand: track.modulation.target = ModulationTargetFromNumber(unsignedNumber); break;
    case ModulationRangeCommand: track.modulation.range = unsignedNumber; break;
    case AttackCommand: track.attack = unsignedNumber; break;
    case DecayCommand: track.decay = unsignedNumber; break;
    case SustainCommand: track.sustain = unsignedNumber; break;
    case ReleaseCommand: track.release = unsignedNumber; break;
    case LoopStartCommand: StartLoop(track, unsignedNumber); break;
    case ExpressionCommand: track.expression = settingNumber; break;
    case SurroundPanCommand: track.surroundPan = SurroundPanFromNumber(unsignedNumber); break;
    case LowPassCutoffCommand: track.lowPassCutoff = unsignedNumber; break;
    case EffectSendACommand: track.effectSendA = settingNumber; break;
    case MainSendCommand: track.mainSend = settingNumber; break;
    case InitialPanCommand: track.initialPan = PanFromNumber(unsignedNumber); break;
    case MuteCommand: MuteTrack(track, unsignedNumber); break;

    case EffectSendBCommand:
    case EffectSendCCommand:
    case PrintVariableCommand:
    case DamperCommand:
        break;
    }
    return false;
}

// The commands with two bytes as their number.
bool SequencePlayer::RunShortCommand(SequenceTrack& track, std::uint8_t command, bool shouldRun, NumberSource source)
{
    std::int32_t number = ReadNumber(track, source, NumberWidth::Short);
    if (!shouldRun)
        return false;

    switch (command)
    {
    case ModulationDelayCommand: track.modulation.delayMilliseconds = std::max(0, number) * ModulationDelayMillisecondsPerNumber; break;
    case TempoCommand: tempo = std::max(0, number); break;
    case SweepPitchCommand: track.sweepSemitones = float(number) * SweepSemitonesPerNumber; break;
    default:
        FailTrack(track);
        return true;
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
        std::int32_t random = RandomNumberBetween(globals, 0, std::abs(number));
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
    if (trackNumber >= tracks.size())
        return;

    SequenceTrack& opened = tracks[trackNumber];
    if (!opened.isAllocated || &opened == &openingTrack)
        return;

    if (opened.isRunning)
        EndTrack(opened);

    opened.position = position;
    opened.waitTicks = 0;
    opened.isRunning = true;
}

void SequencePlayer::ReturnFromCall(SequenceTrack& track)
{
    if (track.returnFrameCount == 0)
        return;

    --track.returnFrameCount;
    track.position = track.returnFrames[track.returnFrameCount].position;
}

void SequencePlayer::StartLoop(SequenceTrack& track, std::uint8_t roundCount)
{
    if (track.returnFrameCount >= MaximumCallDepth)
        return;

    track.returnFrames[track.returnFrameCount] = ReturnFrame{track.position, roundCount};
    ++track.returnFrameCount;
}

// At the end of a round the loop goes back to its start, until it has gone round as many times as it was told. A loop of 0 rounds never ends.
void SequencePlayer::EndLoop(SequenceTrack& track)
{
    if (track.returnFrameCount == 0)
        return;

    ReturnFrame& frame = track.returnFrames[track.returnFrameCount - 1];
    if (frame.loopCount != 0)
    {
        --frame.loopCount;
        if (frame.loopCount == 0)
        {
            --track.returnFrameCount;
            return;
        }
    }
    track.position = frame.position;
}

void SequencePlayer::StartPortamento(SequenceTrack& track, std::uint8_t startKey)
{
    track.isPortamentoOn = true;
    track.portamentoKey = startKey + track.transpose;
}

// A tie that starts or ends lets go of the notes that sound, and the track forgets them.
void SequencePlayer::SetTie(SequenceTrack& track, bool isTied)
{
    track.isTied = isTied;
    ReleaseAndForgetNotes(track);
}

// The modes of the mute command: 0 lets the track sound again, 1 keeps it from starting notes, 2 also lets go of the notes that
// sound, and 3 also stops them at once.
void SequencePlayer::MuteTrack(SequenceTrack& track, std::uint8_t mode)
{
    constexpr std::uint8_t UnmuteMode = 0;
    constexpr std::uint8_t MuteMode = 1;
    constexpr std::uint8_t MuteAndReleaseMode = 2;
    constexpr std::uint8_t MuteAndStopMode = 3;

    switch (mode)
    {
    case UnmuteMode: track.isMuted = false; break;
    case MuteMode: track.isMuted = true; break;
    case MuteAndReleaseMode:
        ReleaseAndForgetNotes(track);
        track.isMuted = true;
        break;
    case MuteAndStopMode:
        StopAndForgetNotes(track);
        track.isMuted = true;
        break;
    default: break;
    }
}

}
