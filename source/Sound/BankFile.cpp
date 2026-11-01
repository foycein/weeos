#include "Sound/BankFile.h"

#include "Sound/Reference.h"
#include "Sound/SoundFileHeader.h"

#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::string_view DataDescription = "Bank file";
constexpr std::uint16_t VersionWithWaves = 0x0101;
constexpr std::uint16_t VersionWithWaveArchive = 0x0102;
constexpr std::size_t HeaderSize = 0x20;
constexpr std::size_t SectionStartSize = 8;
constexpr std::uint8_t HighestNumber = 127;
constexpr std::size_t TableAlignment = 4;

// What lies behind a reference in the data section.
constexpr std::uint8_t SingleNoteType = 1;
constexpr std::uint8_t RangeTableType = 2;
constexpr std::uint8_t IndexTableType = 3;
constexpr std::uint8_t NoNoteType = 4;

// Instruments choose first by the key, and then, inside a key region, by the velocity.
constexpr int KeyLevel = 0;
constexpr int VelocityLevel = 1;

struct NumberRange
{
    std::uint8_t lowest;
    std::uint8_t highest;
};

// The region that is being filled in: the keys and velocities that lead to the note.
struct RegionBounds
{
    NumberRange keys;
    NumberRange velocities;
};

void AddRegions(
    Formats::ByteReader& reader, const Reference& reference, const RegionBounds& bounds, int level, std::vector<NoteRegion>& regions);

RegionBounds NarrowBounds(const RegionBounds& bounds, int level, NumberRange range)
{
    RegionBounds narrowed = bounds;
    if (level == KeyLevel)
        narrowed.keys = range;
    else
        narrowed.velocities = range;
    return narrowed;
}

// A range table has the number of regions, the highest number of each region, and a reference for each region.
// The references start at the next multiple of 4.
void AddRangeRegions(Formats::ByteReader& reader, const RegionBounds& bounds, int level, std::vector<NoteRegion>& regions)
{
    std::size_t tableStart = reader.Position();
    std::uint8_t regionCount = reader.ReadByte();

    std::vector<std::uint8_t> highestNumbers;
    for (std::uint8_t regionNumber = 0; regionNumber < regionCount; ++regionNumber)
        highestNumbers.push_back(reader.ReadByte());

    std::size_t referencesStart = tableStart + Formats::RoundUpToMultiple(sizeof(std::uint8_t) + regionCount, TableAlignment);
    reader.MoveTo(referencesStart);

    std::vector<Reference> references;
    for (std::uint8_t regionNumber = 0; regionNumber < regionCount; ++regionNumber)
        references.push_back(ReadReference(reader));

    std::uint8_t lowestNumber = 0;
    for (std::size_t regionNumber = 0; regionNumber < references.size(); ++regionNumber)
    {
        std::uint8_t highestNumber = highestNumbers[regionNumber];
        if (highestNumber < lowestNumber || highestNumber > HighestNumber)
            reader.Fail(std::format("the range table has the region {} that ends at {}", regionNumber, highestNumber));

        AddRegions(reader, references[regionNumber], NarrowBounds(bounds, level, {lowestNumber, highestNumber}), level + 1, regions);
        lowestNumber = static_cast<std::uint8_t>(highestNumber + 1);
    }
}

// An index table has the lowest and the highest number, and a reference for each number in between.
void AddIndexedRegions(Formats::ByteReader& reader, const RegionBounds& bounds, int level, std::vector<NoteRegion>& regions)
{
    std::uint8_t lowestNumber = reader.ReadByte();
    std::uint8_t highestNumber = reader.ReadByte();
    reader.Skip(sizeof(std::uint16_t));
    if (highestNumber < lowestNumber || highestNumber > HighestNumber)
        reader.Fail(std::format("the index table goes from {} to {}", lowestNumber, highestNumber));

    std::vector<Reference> references;
    for (int number = lowestNumber; number <= highestNumber; ++number)
        references.push_back(ReadReference(reader));

    for (std::size_t referenceNumber = 0; referenceNumber < references.size(); ++referenceNumber)
    {
        std::uint8_t number = static_cast<std::uint8_t>(lowestNumber + referenceNumber);
        AddRegions(reader, references[referenceNumber], NarrowBounds(bounds, level, {number, number}), level + 1, regions);
    }
}

void AddRegions(
    Formats::ByteReader& reader, const Reference& reference, const RegionBounds& bounds, int level, std::vector<NoteRegion>& regions)
{
    if (reference.IsNull() || reference.type == NoNoteType)
        return;

    MoveToTarget(reader, reference, SectionStartSize);
    if (reference.type == SingleNoteType)
    {
        NoteRegion region;
        region.lowestKey = bounds.keys.lowest;
        region.highestKey = bounds.keys.highest;
        region.lowestVelocity = bounds.velocities.lowest;
        region.highestVelocity = bounds.velocities.highest;
        region.note = ReadNoteParameters(reader);
        regions.push_back(region);
        return;
    }

    if (reference.type != RangeTableType && reference.type != IndexTableType)
        reader.Fail(std::format("a reference of the type {} is not a note and not a table", reference.type));

    if (level > VelocityLevel)
        reader.Fail("an instrument has tables inside tables inside tables, only the key and the velocity may choose");

    if (reference.type == RangeTableType)
        AddRangeRegions(reader, bounds, level, regions);
    else
        AddIndexedRegions(reader, bounds, level, regions);
}

std::vector<Instrument> ReadInstruments(Formats::ByteView dataSection)
{
    Formats::ByteReader reader(dataSection, DataDescription);
    reader.Skip(SectionStartSize);
    std::vector<Reference> instrumentReferences = ReadReferenceTable(reader);

    RegionBounds wholeRange = {{0, HighestNumber}, {0, HighestNumber}};
    std::vector<Instrument> instruments;
    for (const Reference& instrumentReference : instrumentReferences)
    {
        Instrument instrument;
        AddRegions(reader, instrumentReference, wholeRange, KeyLevel, instrument.regions);
        instruments.push_back(instrument);
    }
    return instruments;
}

// The wave section lists a reference for each wave. They count from the start of the data of the section.
std::vector<WaveInfo> ReadWaves(Formats::ByteView waveSection)
{
    Formats::ByteReader reader(waveSection, DataDescription);
    reader.Skip(SectionStartSize);
    std::vector<Reference> waveReferences = ReadReferenceTable(reader);

    std::vector<WaveInfo> waves;
    for (const Reference& waveReference : waveReferences)
    {
        MoveToTarget(reader, waveReference, SectionStartSize);
        waves.push_back(ReadWaveInfo(waveSection, reader.Position(), DataDescription));
    }
    return waves;
}

}

std::optional<NoteParameters> BankFile::FindNote(std::size_t instrumentNumber, int key, int velocity) const
{
    if (instrumentNumber >= instruments.size())
        return std::nullopt;

    for (const NoteRegion& region : instruments[instrumentNumber].regions)
    {
        bool keyFits = key >= region.lowestKey && key <= region.highestKey;
        bool velocityFits = velocity >= region.lowestVelocity && velocity <= region.highestVelocity;
        if (keyFits && velocityFits && region.note.waveIndex >= 0)
            return region.note;
    }

    return std::nullopt;
}

void CheckThatTheWavesExist(const BankFile& bankFile, std::size_t waveCount)
{
    for (const Instrument& instrument : bankFile.instruments)
    {
        for (const NoteRegion& region : instrument.regions)
        {
            if (region.note.waveIndex >= 0 && static_cast<std::size_t>(region.note.waveIndex) >= waveCount)
            {
                throw Formats::FormatError(std::format("{}: an instrument plays the wave {}, but there are {} waves", DataDescription,
                    region.note.waveIndex, waveCount));
            }
        }
    }
}

BankFile ReadBankFile(Formats::ByteView bytes)
{
    const KnownVersion knownVersions[] = {{VersionWithWaves, {"DATA", "WAVE"}}, {VersionWithWaveArchive, {"DATA"}}};
    SoundFile file = ReadSoundFileHeader(bytes, "RBNK", knownVersions, HeaderSize, DataDescription);

    BankFile bankFile;
    bankFile.instruments = ReadInstruments(file.sections[0]);
    bankFile.describesItsWaves = file.version == VersionWithWaves;
    if (bankFile.describesItsWaves)
    {
        bankFile.waves = ReadWaves(file.sections[1]);
        CheckThatTheWavesExist(bankFile, bankFile.waves.size());
    }

    return bankFile;
}

}
