#include "Sound/SoundArchiveFile.h"

#include "Sound/Reference.h"
#include "Sound/SoundFileHeader.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::string_view DataDescription = "Sound archive";
constexpr std::uint16_t SupportedVersion = 0x0101;
constexpr std::size_t HeaderSize = 0x40;
constexpr std::size_t SectionStartSize = 8;
constexpr std::uint32_t NoName = 0xFFFFFFFF;

// What a reference in the information section points at, as the sound archive tells it.
constexpr std::uint8_t SequenceRecordType = 1;
constexpr std::uint8_t WaveSoundRecordType = 3;

constexpr std::uint8_t SequenceSoundType = 1;
constexpr std::uint8_t WaveSoundType = 3;

constexpr std::uint8_t DualPanMode = 0;
constexpr std::uint8_t SquareRootPanCurve = 0;

constexpr std::size_t ThreeDimensionalReferenceSize = 8;
constexpr std::size_t UserParametersSize = 8;
constexpr std::size_t UnusedAfterPlayerCountSize = 3;
constexpr std::size_t PlayerHeapAndReservedSize = 8;
constexpr std::size_t BankReservedSize = 4;
constexpr std::size_t ExternalPathReferenceSize = 8;
constexpr std::size_t SymbolTreesSize = 4 * sizeof(std::uint32_t);
constexpr std::size_t GroupEntryReservedSize = 4;
constexpr std::size_t UnusedAfterSequencePrioritiesSize = 2;

// The information section starts with six references. They lead to the tables of sounds, banks, players, files and groups
// and to the limits of the archive. The files are not read: their places are in the groups.
struct InformationReferences
{
    Reference sounds;
    Reference banks;
    Reference players;
    Reference groups;
};

InformationReferences ReadInformationReferences(Formats::ByteReader& reader)
{
    InformationReferences references;
    references.sounds = ReadReference(reader);
    references.banks = ReadReference(reader);
    references.players = ReadReference(reader);
    ReadReference(reader);
    references.groups = ReadReference(reader);
    ReadReference(reader);
    return references;
}

// The symbol section holds the names of everything, one after the other. A name is found by its number.
std::vector<std::string> ReadNames(Formats::ByteView symbolSection)
{
    Formats::ByteReader reader(symbolSection, DataDescription);
    reader.Skip(SectionStartSize);
    std::uint32_t nameTableOffset = reader.ReadUint32();
    reader.Skip(SymbolTreesSize);

    reader.MoveTo(SectionStartSize + nameTableOffset);
    std::uint32_t nameCount = reader.ReadUint32();

    std::vector<std::uint32_t> nameOffsets;
    for (std::uint32_t nameNumber = 0; nameNumber < nameCount; ++nameNumber)
        nameOffsets.push_back(reader.ReadUint32());

    std::vector<std::string> names;
    for (std::uint32_t nameOffset : nameOffsets)
    {
        reader.MoveTo(SectionStartSize + nameOffset);
        names.push_back(reader.ReadTextUntilZero());
    }
    return names;
}

class InformationReader
{
public:
    InformationReader(Formats::ByteView informationSection, const std::vector<std::string>& namesToUse)
        : reader(informationSection, DataDescription)
        , names(namesToUse)
    {
    }

    InformationReferences ReadReferences()
    {
        reader.MoveTo(SectionStartSize);
        return ReadInformationReferences(reader);
    }

    std::vector<SoundInfo> ReadSounds(const Reference& table)
    {
        std::vector<SoundInfo> sounds;
        for (const Reference& record : ReadTable(table))
            sounds.push_back(ReadSound(record));
        return sounds;
    }

    std::vector<BankInfo> ReadBanks(const Reference& table)
    {
        std::vector<BankInfo> banks;
        for (const Reference& record : ReadTable(table))
            banks.push_back(ReadBank(record));
        return banks;
    }

    std::vector<PlayerInfo> ReadPlayers(const Reference& table)
    {
        std::vector<PlayerInfo> players;
        for (const Reference& record : ReadTable(table))
            players.push_back(ReadPlayer(record));
        return players;
    }

    std::vector<GroupInfo> ReadGroups(const Reference& table)
    {
        std::vector<GroupInfo> groups;
        for (const Reference& record : ReadTable(table))
            groups.push_back(ReadGroup(record));
        return groups;
    }

private:
    std::vector<Reference> ReadTable(const Reference& table)
    {
        MoveToTarget(reader, table, SectionStartSize);
        return ReadReferenceTable(reader);
    }

    void MoveToRecord(const Reference& record)
    {
        MoveToTarget(reader, record, SectionStartSize);
    }

    std::string ReadName()
    {
        std::uint32_t nameNumber = reader.ReadUint32();
        if (nameNumber == NoName)
            return std::string();

        if (nameNumber >= names.size())
            reader.Fail(std::format("the name {} does not exist, the archive has {} names", nameNumber, names.size()));

        return names[nameNumber];
    }

    SequenceSettings ReadSequenceSettings()
    {
        SequenceSettings settings;
        settings.startOffset = reader.ReadUint32();
        settings.bankNumber = reader.ReadUint32();
        settings.trackMask = reader.ReadUint32();
        settings.channelPriority = reader.ReadByte();
        settings.releasePriorityFix = reader.ReadByte();
        reader.Skip(UnusedAfterSequencePrioritiesSize);
        return settings;
    }

    WaveSoundSettings ReadWaveSoundSettings()
    {
        WaveSoundSettings settings;
        settings.soundNumber = reader.ReadUint32();
        settings.trackMask = reader.ReadUint32();
        settings.channelPriority = reader.ReadByte();
        settings.releasePriorityFix = reader.ReadByte();
        return settings;
    }

    // The kind of a sound is given twice: in the sound record and in the type of the reference to its settings.
    void ReadSettings(SoundInfo& sound, std::uint8_t soundType, const Reference& settings)
    {
        MoveToRecord(settings);
        if (soundType == SequenceSoundType && settings.type == SequenceRecordType)
        {
            sound.kind = SoundKind::Sequence;
            sound.sequence = ReadSequenceSettings();
        }
        else if (soundType == WaveSoundType && settings.type == WaveSoundRecordType)
        {
            sound.kind = SoundKind::WaveSound;
            sound.waveSound = ReadWaveSoundSettings();
        }
        else
        {
            reader.Fail(std::format("the sound {} has the type {} and settings of the type {}, only sequences and wave sounds are supported",
                sound.name, soundType, settings.type));
        }
    }

    SoundInfo ReadSound(const Reference& record)
    {
        MoveToRecord(record);

        SoundInfo sound{};
        sound.name = ReadName();
        sound.fileId = reader.ReadUint32();
        sound.playerNumber = reader.ReadUint32();
        reader.Skip(ThreeDimensionalReferenceSize);
        sound.volume = reader.ReadByte();
        sound.playerPriority = reader.ReadByte();
        std::uint8_t soundType = reader.ReadByte();
        reader.Skip(sizeof(std::uint8_t));
        Reference settings = ReadReference(reader);
        reader.Skip(UserParametersSize);

        std::uint8_t panMode = reader.ReadByte();
        std::uint8_t panCurve = reader.ReadByte();
        if (panMode != DualPanMode || panCurve != SquareRootPanCurve)
            reader.Fail(std::format("the sound {} uses the pan mode {} and the pan curve {}, which are not supported", sound.name, panMode, panCurve));

        ReadSettings(sound, soundType, settings);
        return sound;
    }

    BankInfo ReadBank(const Reference& record)
    {
        MoveToRecord(record);

        BankInfo bank;
        bank.name = ReadName();
        bank.fileId = reader.ReadUint32();
        reader.Skip(BankReservedSize);
        return bank;
    }

    PlayerInfo ReadPlayer(const Reference& record)
    {
        MoveToRecord(record);

        PlayerInfo player;
        player.name = ReadName();
        player.maximumSoundCount = reader.ReadByte();
        reader.Skip(UnusedAfterPlayerCountSize + PlayerHeapAndReservedSize);
        return player;
    }

    GroupEntry ReadGroupEntry(const Reference& record)
    {
        MoveToRecord(record);

        GroupEntry entry;
        entry.fileId = reader.ReadUint32();
        entry.fileOffset = reader.ReadUint32();
        entry.fileSize = reader.ReadUint32();
        entry.waveDataOffset = reader.ReadUint32();
        entry.waveDataSize = reader.ReadUint32();
        reader.Skip(GroupEntryReservedSize);
        return entry;
    }

    GroupInfo ReadGroup(const Reference& record)
    {
        MoveToRecord(record);

        GroupInfo group;
        group.name = ReadName();
        reader.Skip(sizeof(std::int32_t) + ExternalPathReferenceSize);
        group.fileBlockOffset = reader.ReadUint32();
        group.fileBlockSize = reader.ReadUint32();
        group.waveBlockOffset = reader.ReadUint32();
        group.waveBlockSize = reader.ReadUint32();
        Reference entryTable = ReadReference(reader);

        for (const Reference& entryRecord : ReadTable(entryTable))
            group.entries.push_back(ReadGroupEntry(entryRecord));

        return group;
    }

    Formats::ByteReader reader;
    const std::vector<std::string>& names;
};

void CheckGroup(const GroupInfo& group, std::size_t archiveSize)
{
    std::string_view name = group.name;
    if (!Formats::IsRangeInside(group.fileBlockOffset, group.fileBlockSize, archiveSize) ||
        !Formats::IsRangeInside(group.waveBlockOffset, group.waveBlockSize, archiveSize))
    {
        throw Formats::FormatError(std::format("{}: the blocks of the group {} lie outside the archive", DataDescription, name));
    }

    for (const GroupEntry& entry : group.entries)
    {
        if (!Formats::IsRangeInside(entry.fileOffset, entry.fileSize, group.fileBlockSize) ||
            !Formats::IsRangeInside(entry.waveDataOffset, entry.waveDataSize, group.waveBlockSize))
        {
            throw Formats::FormatError(std::format("{}: the file {} lies outside the blocks of the group {}", DataDescription, entry.fileId, name));
        }
    }
}

void CheckReferencesBetweenTables(const SoundArchiveFile& archive)
{
    for (const SoundInfo& sound : archive.sounds)
    {
        if (sound.playerNumber >= archive.players.size())
        {
            throw Formats::FormatError(std::format("{}: the sound {} uses the player {}, but there are {} players", DataDescription,
                sound.name, sound.playerNumber, archive.players.size()));
        }

        if (sound.kind == SoundKind::Sequence && sound.sequence.bankNumber >= archive.banks.size())
        {
            throw Formats::FormatError(std::format("{}: the sound {} uses the bank {}, but there are {} banks", DataDescription,
                sound.name, sound.sequence.bankNumber, archive.banks.size()));
        }
    }

    for (const GroupInfo& group : archive.groups)
        CheckGroup(group, archive.bytes.size());
}

}

std::optional<std::size_t> SoundArchiveFile::FindSoundNumber(std::string_view name) const
{
    for (std::size_t soundNumber = 0; soundNumber < sounds.size(); ++soundNumber)
    {
        if (sounds[soundNumber].name == name)
            return soundNumber;
    }

    return std::nullopt;
}

EmbeddedFile SoundArchiveFile::FindFile(std::uint32_t fileId) const&
{
    Formats::ByteView archiveBytes(bytes);

    for (const GroupInfo& group : groups)
    {
        for (const GroupEntry& entry : group.entries)
        {
            if (entry.fileId != fileId)
                continue;

            EmbeddedFile file;
            file.fileBytes = archiveBytes.subspan(std::size_t(group.fileBlockOffset) + entry.fileOffset, entry.fileSize);
            file.waveData = archiveBytes.subspan(std::size_t(group.waveBlockOffset) + entry.waveDataOffset, entry.waveDataSize);
            return file;
        }
    }

    throw Formats::FormatError(std::format("{}: no group has the file {}", DataDescription, fileId));
}

SoundArchiveFile ReadSoundArchiveFile(Formats::ByteView bytes)
{
    SoundFile file = ReadSoundFileHeader(bytes, "RSAR", SupportedVersion, HeaderSize, {"SYMB", "INFO", "FILE"}, DataDescription);

    std::vector<std::string> names = ReadNames(file.sections[0]);
    InformationReader information(file.sections[1], names);
    InformationReferences references = information.ReadReferences();

    SoundArchiveFile archive;
    archive.bytes.assign(bytes.begin(), bytes.end());
    archive.sounds = information.ReadSounds(references.sounds);
    archive.banks = information.ReadBanks(references.banks);
    archive.players = information.ReadPlayers(references.players);
    archive.groups = information.ReadGroups(references.groups);

    CheckReferencesBetweenTables(archive);
    return archive;
}

}
