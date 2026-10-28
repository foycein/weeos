#include "Sound/NoteParameters.h"

#include "Sound/Reference.h"

namespace Sound
{

namespace
{

// The library of the System Menu has no hold stage in its envelope, so the four bytes after the envelope are not used.
constexpr std::size_t UnusedAfterEnvelopeSize = 4;
constexpr std::size_t UnusedAtEndSize = 4;

}

NoteParameters ReadNoteParameters(Formats::ByteReader& reader)
{
    NoteParameters note;
    note.waveIndex = static_cast<std::int32_t>(reader.ReadUint32());
    note.attack = reader.ReadByte();
    note.decay = reader.ReadByte();
    note.sustain = reader.ReadByte();
    note.release = reader.ReadByte();
    reader.Skip(UnusedAfterEnvelopeSize);
    note.originalKey = reader.ReadByte();
    note.volume = reader.ReadByte();
    note.pan = reader.ReadByte();
    note.surroundPan = reader.ReadByte();
    note.tune = reader.ReadFloat();

    Reference vibratoTable = ReadReference(reader);
    Reference volumeCurveTable = ReadReference(reader);
    Reference randomChangeTable = ReadReference(reader);
    if (!vibratoTable.IsNull() || !volumeCurveTable.IsNull() || !randomChangeTable.IsNull())
        reader.Fail("a note uses vibrato, a volume curve or random changes, which are not supported");

    reader.Skip(UnusedAtEndSize);
    return note;
}

}
