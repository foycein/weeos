#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <vector>

namespace Sound
{

// What a MIDI event of a channel does. Other events (aftertouch, pitch wheel, system messages) are read over and left out.
enum class MidiEventKind
{
    NoteOff,
    NoteOn,
    Controller,
    ProgramChange,
};

struct MidiEvent
{
    // From the start of the file, with the tempo changes of the file counted in.
    double seconds = 0;

    MidiEventKind kind = MidiEventKind::NoteOn;
    std::uint8_t channel = 0;

    // The key of a note, the number of a controller or of a program.
    std::uint8_t number = 0;

    // The velocity of a note or the value of a controller. A note on with velocity 0 is read as a note off.
    std::uint8_t value = 0;
};

// A Standard MIDI File of format 0 or 1, as the HOME menu of the Internet Channel keeps its sounds (homebutton/HomeButtonSe.arc).
struct MidiFile
{
    // The events of every track, in the order they happen.
    std::vector<MidiEvent> events;

    // When the last track ends.
    double lengthSeconds = 0;
};

// Throws a FormatError when the file is not a MIDI file or ends inside an event.
MidiFile ReadMidiFile(Formats::ByteView bytes);

}
