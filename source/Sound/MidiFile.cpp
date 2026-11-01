#include "Sound/MidiFile.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <optional>

namespace Sound
{

namespace
{

constexpr std::uint32_t HeaderTag = 0x4D546864;
constexpr std::uint32_t TrackTag = 0x4D54726B;
constexpr std::uint16_t HighestFormat = 1;

// A tempo is the length of a quarter note; without a tempo event it is 120 beats a minute.
constexpr double DefaultSecondsPerQuarterNote = 0.5;
constexpr double MicrosecondsPerSecond = 1000000.0;

constexpr std::uint8_t StatusBit = 0x80;
constexpr std::uint8_t ChannelMask = 0x0F;
constexpr std::uint8_t MetaEvent = 0xFF;
constexpr std::uint8_t SystemExclusive = 0xF0;
constexpr std::uint8_t SystemExclusiveEnd = 0xF7;
constexpr std::uint8_t TempoMeta = 0x51;
constexpr std::uint8_t EndOfTrackMeta = 0x2F;
constexpr std::uint16_t TimeCodeDivisionBit = 0x8000;

enum class MessageKind : std::uint8_t
{
    NoteOff = 0x80,
    NoteOn = 0x90,
    KeyPressure = 0xA0,
    Controller = 0xB0,
    ProgramChange = 0xC0,
    ChannelPressure = 0xD0,
    PitchWheel = 0xE0,
};

// An event at its tick: a channel event or a change of the tempo.
struct TickedEvent
{
    std::uint32_t tick = 0;
    std::optional<MidiEvent> event;
    std::optional<double> secondsPerQuarterNote;
};

std::uint32_t ReadVariableLength(Formats::ByteReader& reader)
{
    constexpr std::uint8_t MoreBytesBit = 0x80;
    constexpr std::uint8_t ValueBits = 0x7F;
    constexpr int ValueBitCount = 7;
    constexpr int MostBytes = 4;

    std::uint32_t value = 0;
    for (int count = 0; count < MostBytes; ++count)
    {
        std::uint8_t byte = reader.ReadByte();
        value = (value << ValueBitCount) | (byte & ValueBits);
        if ((byte & MoreBytesBit) == 0)
            return value;
    }
    throw Formats::FormatError("A MIDI number is longer than four bytes");
}

std::optional<MidiEvent> MakeChannelEvent(MessageKind kind, std::uint8_t channel, std::uint8_t first, std::uint8_t second)
{
    MidiEvent event;
    event.channel = channel;
    event.number = first;
    event.value = second;
    switch (kind)
    {
    case MessageKind::NoteOff:
        event.kind = MidiEventKind::NoteOff;
        return event;
    case MessageKind::NoteOn:
        event.kind = second == 0 ? MidiEventKind::NoteOff : MidiEventKind::NoteOn;
        return event;
    case MessageKind::Controller:
        event.kind = MidiEventKind::Controller;
        return event;
    case MessageKind::ProgramChange:
        event.kind = MidiEventKind::ProgramChange;
        return event;
    default:
        return std::nullopt;
    }
}

bool HasOneDataByte(MessageKind kind)
{
    return kind == MessageKind::ProgramChange || kind == MessageKind::ChannelPressure;
}

void ReadMetaEvent(Formats::ByteReader& reader, std::uint32_t tick, std::vector<TickedEvent>& events, bool& hasEnded)
{
    std::uint8_t type = reader.ReadByte();
    std::uint32_t length = ReadVariableLength(reader);
    Formats::ByteView data = reader.ReadBytes(length);
    if (type == EndOfTrackMeta)
        hasEnded = true;

    constexpr std::uint32_t TempoLength = 3;
    if (type != TempoMeta || length != TempoLength)
        return;

    // The length of a quarter note in microseconds, as a big-endian number of three bytes.
    constexpr int BitsPerByte = 8;
    std::uint32_t microseconds = 0;
    for (std::uint8_t byte : data)
        microseconds = (microseconds << BitsPerByte) | byte;
    events.push_back({tick, std::nullopt, microseconds / MicrosecondsPerSecond});
}

// The events of one track, at their ticks. A status byte may be left out when it is the same as the one before (running status).
void ReadTrack(Formats::ByteView track, std::vector<TickedEvent>& events, std::uint32_t& lastTick)
{
    Formats::ByteReader reader(track, "MIDI track");
    std::uint32_t tick = 0;
    std::uint8_t runningStatus = 0;
    bool hasEnded = false;
    while (!hasEnded && reader.Position() < track.size())
    {
        tick += ReadVariableLength(reader);
        std::uint8_t status = reader.ReadByte();
        if (status == MetaEvent)
        {
            ReadMetaEvent(reader, tick, events, hasEnded);
            continue;
        }
        if (status == SystemExclusive || status == SystemExclusiveEnd)
        {
            reader.Skip(ReadVariableLength(reader));
            continue;
        }

        std::uint8_t first = 0;
        if ((status & StatusBit) == 0)
        {
            if (runningStatus == 0)
                throw Formats::FormatError("A MIDI event has no status");
            first = status;
            status = runningStatus;
        }
        else
        {
            runningStatus = status;
            first = reader.ReadByte();
        }

        MessageKind kind = static_cast<MessageKind>(status & ~ChannelMask);
        std::uint8_t second = HasOneDataByte(kind) ? 0 : reader.ReadByte();
        std::optional<MidiEvent> event = MakeChannelEvent(kind, status & ChannelMask, first, second);
        if (event)
            events.push_back({tick, event, std::nullopt});
    }
    lastTick = std::max(lastTick, tick);
}

// Turns the ticks of the events of all tracks into seconds, following the changes of the tempo.
MidiFile TimeTheEvents(std::vector<TickedEvent>& tickedEvents, std::uint16_t ticksPerQuarterNote, std::uint32_t lastTick)
{
    std::stable_sort(tickedEvents.begin(), tickedEvents.end(),
        [](const TickedEvent& first, const TickedEvent& second) { return first.tick < second.tick; });

    MidiFile file;
    double secondsPerTick = DefaultSecondsPerQuarterNote / ticksPerQuarterNote;
    double seconds = 0;
    std::uint32_t tick = 0;
    for (const TickedEvent& ticked : tickedEvents)
    {
        seconds += (ticked.tick - tick) * secondsPerTick;
        tick = ticked.tick;
        if (ticked.secondsPerQuarterNote)
            secondsPerTick = *ticked.secondsPerQuarterNote / ticksPerQuarterNote;
        if (!ticked.event)
            continue;

        MidiEvent event = *ticked.event;
        event.seconds = seconds;
        file.events.push_back(event);
    }
    file.lengthSeconds = seconds + (lastTick - tick) * secondsPerTick;
    return file;
}

}

MidiFile ReadMidiFile(Formats::ByteView bytes)
{
    Formats::ByteReader reader(bytes, "MIDI file");
    if (reader.ReadUint32() != HeaderTag)
        throw Formats::FormatError("Not a MIDI file");

    std::uint32_t headerLength = reader.ReadUint32();
    std::size_t afterHeader = reader.Position() + headerLength;
    std::uint16_t format = reader.ReadUint16();
    std::uint16_t trackCount = reader.ReadUint16();
    std::uint16_t ticksPerQuarterNote = reader.ReadUint16();
    if (format > HighestFormat)
        throw Formats::FormatError("Only MIDI files of format 0 and 1 are read");

    bool countsInTimeCode = (ticksPerQuarterNote & TimeCodeDivisionBit) != 0;
    if (countsInTimeCode || ticksPerQuarterNote == 0)
        throw Formats::FormatError("Only MIDI files that count in quarter notes are read");
    reader.MoveTo(afterHeader);

    std::vector<TickedEvent> tickedEvents;
    std::uint32_t lastTick = 0;
    for (std::uint16_t track = 0; track < trackCount; ++track)
    {
        if (reader.ReadUint32() != TrackTag)
            throw Formats::FormatError("A MIDI track has no MTrk tag");
        std::uint32_t trackLength = reader.ReadUint32();
        ReadTrack(reader.ReadBytes(trackLength), tickedEvents, lastTick);
    }
    return TimeTheEvents(tickedEvents, ticksPerQuarterNote, lastTick);
}

}
