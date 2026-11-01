#include "Sound/WaveSoundFile.h"

#include "Sound/Reference.h"
#include "Sound/SoundFileHeader.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <format>

namespace Sound
{

namespace
{

constexpr std::string_view DataDescription = "Wave sound file";
constexpr std::uint16_t VersionWithWaves = 0x0102;
constexpr std::uint16_t VersionWithWaveArchive = 0x0103;
constexpr std::size_t HeaderSize = 0x20;
constexpr std::size_t SectionStartSize = 8;
constexpr std::size_t UnusedAfterSendsSize = 2;
constexpr std::size_t SendsToOtherBusesSize = 2;
constexpr std::size_t TablesAfterSendsSize = 20;

// The three parts that make up a wave sound: its description, the list of its tracks and the list of its notes.
struct WaveSoundReferences
{
    Reference info;
    Reference tracks;
    Reference notes;
};

WaveSoundReferences ReadWaveSoundReferences(Formats::ByteReader& reader)
{
    WaveSoundReferences references;
    references.info = ReadReference(reader);
    references.tracks = ReadReference(reader);
    references.notes = ReadReference(reader);
    return references;
}

void ReadInfo(Formats::ByteReader& reader, WaveSound& sound)
{
    sound.pitch = reader.ReadFloat();
    sound.pan = reader.ReadByte();
    sound.surroundPan = reader.ReadByte();
    sound.effectSendA = reader.ReadByte();
    reader.Skip(SendsToOtherBusesSize);
    sound.mainSend = reader.ReadByte();
    reader.Skip(UnusedAfterSendsSize);

    Formats::ByteView tables = reader.ReadBytes(TablesAfterSendsSize);
    bool tablesAreEmpty = std::all_of(tables.begin(), tables.end(), [](Formats::Byte tableByte) { return tableByte == 0; });
    if (!tablesAreEmpty)
        reader.Fail("a wave sound uses vibrato, a volume curve or random changes, which are not supported");
}

// A track tells when its notes start and how long they last. All of the menu's sounds have one track with one note
// that starts at 0 and lasts "until the wave ends" (a length of 0). Returns the number of that note.
std::uint32_t ReadOnlyNoteNumber(Formats::ByteReader& reader, std::size_t dataStart, const Reference& tracks)
{
    MoveToTarget(reader, tracks, dataStart);
    std::vector<Reference> trackReferences = ReadReferenceTable(reader);
    if (trackReferences.size() != 1)
        reader.Fail(std::format("a wave sound has {} tracks, but only sounds with one track are supported", trackReferences.size()));

    MoveToTarget(reader, trackReferences[0], dataStart);
    Reference eventTable = ReadReference(reader);

    MoveToTarget(reader, eventTable, dataStart);
    std::vector<Reference> eventReferences = ReadReferenceTable(reader);
    if (eventReferences.size() != 1)
        reader.Fail(std::format("a wave sound track has {} notes, but only tracks with one note are supported", eventReferences.size()));

    MoveToTarget(reader, eventReferences[0], dataStart);
    float position = reader.ReadFloat();
    float length = reader.ReadFloat();
    std::uint32_t noteNumber = reader.ReadUint32();
    if (position != 0.0f || length != 0.0f)
        reader.Fail("a wave sound note does not start at the beginning or does not last to the end of the wave");

    return noteNumber;
}

NoteParameters ReadNote(Formats::ByteReader& reader, std::size_t dataStart, const Reference& notes, std::uint32_t noteNumber)
{
    MoveToTarget(reader, notes, dataStart);
    std::vector<Reference> noteReferences = ReadReferenceTable(reader);
    if (noteNumber >= noteReferences.size())
        reader.Fail(std::format("the note {} does not exist, the sound has {} notes", noteNumber, noteReferences.size()));

    MoveToTarget(reader, noteReferences[noteNumber], dataStart);
    return ReadNoteParameters(reader);
}

WaveSound ReadWaveSound(Formats::ByteReader& reader, std::size_t dataStart, const Reference& soundReference)
{
    MoveToTarget(reader, soundReference, dataStart);
    WaveSoundReferences references = ReadWaveSoundReferences(reader);

    WaveSound sound;
    MoveToTarget(reader, references.info, dataStart);
    ReadInfo(reader, sound);

    std::uint32_t noteNumber = ReadOnlyNoteNumber(reader, dataStart, references.tracks);
    sound.note = ReadNote(reader, dataStart, references.notes, noteNumber);
    return sound;
}

std::vector<WaveSound> ReadSounds(Formats::ByteView dataSection)
{
    Formats::ByteReader reader(dataSection, DataDescription);
    reader.Skip(SectionStartSize);
    std::vector<Reference> soundReferences = ReadReferenceTable(reader);

    std::vector<WaveSound> sounds;
    for (const Reference& soundReference : soundReferences)
        sounds.push_back(ReadWaveSound(reader, SectionStartSize, soundReference));

    return sounds;
}

std::vector<WaveInfo> ReadWaves(Formats::ByteView waveSection)
{
    Formats::ByteReader reader(waveSection, DataDescription);
    reader.Skip(SectionStartSize);
    std::uint32_t waveCount = reader.ReadUint32();

    std::vector<std::uint32_t> waveOffsets;
    for (std::uint32_t waveNumber = 0; waveNumber < waveCount; ++waveNumber)
        waveOffsets.push_back(reader.ReadUint32());

    std::vector<WaveInfo> waves;
    for (std::uint32_t waveOffset : waveOffsets)
        waves.push_back(ReadWaveInfo(waveSection, waveOffset, DataDescription));

    return waves;
}

}

void CheckThatTheWavesExist(const WaveSoundFile& waveSoundFile, std::size_t waveCount)
{
    for (const WaveSound& sound : waveSoundFile.sounds)
    {
        if (sound.note.waveIndex < 0 || static_cast<std::size_t>(sound.note.waveIndex) >= waveCount)
        {
            throw Formats::FormatError(std::format("{}: a sound plays the wave {}, but there are {} waves", DataDescription,
                sound.note.waveIndex, waveCount));
        }
    }
}

WaveSoundFile ReadWaveSoundFile(Formats::ByteView bytes)
{
    const KnownVersion knownVersions[] = {{VersionWithWaves, {"DATA", "WAVE"}}, {VersionWithWaveArchive, {"DATA"}}};
    SoundFile file = ReadSoundFileHeader(bytes, "RWSD", knownVersions, HeaderSize, DataDescription);

    WaveSoundFile waveSoundFile;
    waveSoundFile.sounds = ReadSounds(file.sections[0]);
    waveSoundFile.describesItsWaves = file.version == VersionWithWaves;
    if (waveSoundFile.describesItsWaves)
    {
        waveSoundFile.waves = ReadWaves(file.sections[1]);
        CheckThatTheWavesExist(waveSoundFile, waveSoundFile.waves.size());
    }

    return waveSoundFile;
}

}
