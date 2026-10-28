#include "Formats/NandFileSystem.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"
#include "Formats/HmacSha1.h"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

namespace Formats
{

namespace
{

constexpr std::string_view SuperblockMagic = "SFFS";
constexpr std::size_t SuperblockMagicSize = 4;

// The last 256 clusters hold 16 superblocks, one every 16 clusters. They take turns, so the newest one has the highest generation.
constexpr std::uint32_t FirstSuperblockCluster = 0x7F00;
constexpr std::uint32_t SuperblockSlotCount = 16;
constexpr std::uint32_t ClustersPerSuperblock = 16;

// The data of the files lies behind the boot blocks and in front of the superblocks.
constexpr std::uint32_t FirstDataCluster = 0x40;
constexpr std::uint32_t DataClusterLimit = FirstSuperblockCluster;

// In a superblock the magic and the generation are followed by one more number, then comes the table that chains the
// clusters (two bytes for each cluster of the NAND) and then the tree of the files and directories.
constexpr std::size_t ClusterChainsOffset = 0x0C;
constexpr std::size_t TreeOffset = ClusterChainsOffset + NandClusterCount * sizeof(std::uint16_t);
constexpr std::size_t TreeRecordCount = 0x17FF;
constexpr std::size_t NameFieldSize = 12;

// The last four bytes of a tree record are not known. They only go into the HMAC of the clusters.
constexpr std::size_t UnknownFieldSize = 4;

// In front of the data of a cluster, the HMAC covers 64 bytes: the owner, the name, the place of the cluster in the file,
// the number of the tree record and the unknown field of the record, then zeros. The superblock has the number of its first cluster
// in the place and zeros in the rest.
constexpr std::size_t HmacSaltSize = 0x40;

// The spare bytes of the 7th and 8th page of a cluster hold the HMAC twice, each time behind one byte for the mark of a bad block.
// The first copy is in the 7th page. The second copy starts behind it with 12 bytes and ends with 8 bytes in the 8th page.
constexpr std::uint32_t HmacPageInCluster = 6;
constexpr std::size_t FirstCopyOffset = 1;
constexpr std::size_t SecondCopyOffset = FirstCopyOffset + Sha1DigestSize;
constexpr std::size_t SecondCopySizeInFirstPage = 12;
constexpr std::size_t SecondCopyOffsetInNextPage = 1;

constexpr std::uint16_t NoRecord = 0xFFFF;
constexpr std::size_t RootRecordNumber = 0;
constexpr std::string_view RootPath = "/";

// The mode byte holds the rights of the owner (bits 7 and 6), the group (5 and 4), everybody else (3 and 2) and the kind (1 and 0).
constexpr std::uint8_t KindMask = 0x03;
constexpr std::uint8_t FileKindNumber = 1;
constexpr std::uint8_t DirectoryKindNumber = 2;
constexpr int OwnerRightsShift = 6;
constexpr int GroupRightsShift = 4;
constexpr int OtherRightsShift = 2;
constexpr std::uint8_t RightsMask = 0x03;

constexpr char FirstNameCharacter = ' ';
constexpr char LastNameCharacter = '~';

struct Superblock
{
    std::uint32_t firstCluster;
    std::uint32_t generation;
    Bytes bytes;
};

// One record of the tree as it is stored. A file points to its first cluster, a directory to the first record inside it,
// and every record points to the next record in the same directory.
struct TreeRecord
{
    std::string name;
    std::uint8_t mode;
    std::uint8_t attributes;
    std::uint16_t firstClusterOrEntry;
    std::uint16_t nextRecord;
    std::uint32_t size;
    std::uint32_t userId;
    std::uint16_t groupId;
    std::array<Byte, UnknownFieldSize> unknownField;
};

// Gives the generation of the superblock that starts at the cluster, or nothing when there is none.
std::optional<std::uint32_t> ReadGeneration(NandImage& image, std::uint32_t firstCluster)
{
    Bytes firstPage = image.ReadPageData(firstCluster * NandPagesPerCluster);
    ByteReader reader(firstPage, "Superblock");

    if (reader.ReadText(SuperblockMagicSize) != SuperblockMagic)
        return std::nullopt;

    return reader.ReadUint32();
}

Superblock ReadNewestSuperblock(NandImage& image)
{
    std::optional<std::uint32_t> newestCluster;
    std::uint32_t newestGeneration = 0;

    for (std::uint32_t slot = 0; slot < SuperblockSlotCount; ++slot)
    {
        std::uint32_t firstCluster = FirstSuperblockCluster + slot * ClustersPerSuperblock;
        std::optional<std::uint32_t> generation = ReadGeneration(image, firstCluster);
        if (!generation)
            continue;

        if (!newestCluster || *generation > newestGeneration)
        {
            newestCluster = firstCluster;
            newestGeneration = *generation;
        }
    }

    if (!newestCluster)
        throw FormatError("NAND image: no superblock of the file system was found");

    Superblock superblock;
    superblock.firstCluster = *newestCluster;
    superblock.generation = newestGeneration;
    for (std::uint32_t cluster = 0; cluster < ClustersPerSuperblock; ++cluster)
    {
        Bytes clusterData = image.ReadClusterData(*newestCluster + cluster);
        superblock.bytes.insert(superblock.bytes.end(), clusterData.begin(), clusterData.end());
    }
    return superblock;
}

std::vector<std::uint16_t> ReadClusterChains(const Bytes& superblockBytes)
{
    ByteReader reader(superblockBytes, "Superblock");
    reader.MoveTo(ClusterChainsOffset);

    std::vector<std::uint16_t> clusterChains;
    for (std::uint32_t cluster = 0; cluster < NandClusterCount; ++cluster)
        clusterChains.push_back(reader.ReadUint16());

    return clusterChains;
}

TreeRecord ReadTreeRecord(ByteReader& reader)
{
    TreeRecord record;
    record.name = reader.ReadText(NameFieldSize);
    record.mode = reader.ReadByte();
    record.attributes = reader.ReadByte();
    record.firstClusterOrEntry = reader.ReadUint16();
    record.nextRecord = reader.ReadUint16();
    record.size = reader.ReadUint32();
    record.userId = reader.ReadUint32();
    record.groupId = reader.ReadUint16();
    record.unknownField = reader.ReadByteArray<UnknownFieldSize>();
    return record;
}

std::vector<TreeRecord> ReadTreeRecords(const Bytes& superblockBytes)
{
    ByteReader reader(superblockBytes, "Superblock");
    reader.MoveTo(TreeOffset);

    std::vector<TreeRecord> records;
    for (std::size_t number = 0; number < TreeRecordCount; ++number)
        records.push_back(ReadTreeRecord(reader));

    return records;
}

NandRights RightsOf(std::uint8_t mode, int shift)
{
    return static_cast<NandRights>((mode >> shift) & RightsMask);
}

bool IsValidName(const std::string& name)
{
    if (name.empty() || name == "." || name == "..")
        return false;

    return std::all_of(name.begin(), name.end(),
        [](char character) { return character >= FirstNameCharacter && character <= LastNameCharacter && character != '/'; });
}

// The 64 bytes that the HMAC of a cluster covers in front of its data.
Bytes MakeHmacSalt(std::uint32_t userId, const std::string& name, std::uint32_t clusterField, std::uint32_t recordNumber,
    const std::array<Byte, UnknownFieldSize>& unknownField)
{
    Bytes nameField(NameFieldSize, Byte(0));
    std::copy(name.begin(), name.end(), nameField.begin());

    ByteWriter writer;
    writer.WriteUint32(userId);
    writer.WriteBytes(nameField);
    writer.WriteUint32(clusterField);
    writer.WriteUint32(recordNumber);
    writer.WriteBytes(unknownField);
    writer.WriteZeros(HmacSaltSize - writer.GetBytes().size());
    return writer.GetBytes();
}

Bytes JoinBytes(ByteView first, ByteView second)
{
    Bytes joined(first.begin(), first.end());
    joined.insert(joined.end(), second.begin(), second.end());
    return joined;
}

std::string JoinPath(const std::string& directoryPath, const std::string& name)
{
    if (directoryPath == RootPath)
        return directoryPath + name;

    return directoryPath + "/" + name;
}

// Turns the records into entries, walking the tree from the root: first the records inside a directory, one after the other.
class TreeBuilder
{
public:
    explicit TreeBuilder(const std::vector<TreeRecord>& fstRecords)
        : records(fstRecords)
        , isReached(fstRecords.size(), false)
    {
    }

    std::vector<NandEntry> Build()
    {
        const TreeRecord& root = records[RootRecordNumber];
        isReached[RootRecordNumber] = true;

        NandEntry rootEntry = MakeEntry(root, RootRecordNumber, std::string(RootPath), std::string(RootPath));
        if (rootEntry.kind != NandEntryKind::Directory)
            throw FormatError("NAND file system: the first record is not the root directory");

        entries.push_back(rootEntry);
        AddEntriesInside(root, rootEntry.path);
        return entries;
    }

private:
    void AddEntriesInside(const TreeRecord& directory, const std::string& directoryPath)
    {
        std::uint16_t recordNumber = directory.firstClusterOrEntry;
        while (recordNumber != NoRecord)
        {
            const TreeRecord& record = ReachRecord(recordNumber);
            RequireValidName(record, recordNumber);

            NandEntry entry = MakeEntry(record, recordNumber, record.name, JoinPath(directoryPath, record.name));
            entries.push_back(entry);
            if (entry.kind == NandEntryKind::Directory)
                AddEntriesInside(record, entry.path);

            recordNumber = record.nextRecord;
        }
    }

    // Each record may be reached once. Reaching it twice would mean the tree has a loop or two directories share an entry.
    const TreeRecord& ReachRecord(std::uint16_t recordNumber)
    {
        if (recordNumber >= records.size())
            throw FormatError(std::format("NAND file system: a record points to the record {}, which does not exist", recordNumber));

        if (isReached[recordNumber])
            throw FormatError(std::format("NAND file system: the record {} is reached twice, the tree has a loop", recordNumber));

        isReached[recordNumber] = true;
        return records[recordNumber];
    }

    static void RequireValidName(const TreeRecord& record, std::size_t recordNumber)
    {
        if (!IsValidName(record.name))
            throw FormatError(std::format("NAND file system: the record {} has an invalid name", recordNumber));
    }

    static NandEntry MakeEntry(const TreeRecord& record, std::size_t recordNumber, std::string name, std::string path)
    {
        NandEntry entry;
        entry.path = std::move(path);
        entry.name = std::move(name);
        entry.kind = KindOf(record, recordNumber);
        entry.ownerRights = RightsOf(record.mode, OwnerRightsShift);
        entry.groupRights = RightsOf(record.mode, GroupRightsShift);
        entry.otherRights = RightsOf(record.mode, OtherRightsShift);
        entry.attributes = record.attributes;
        entry.userId = record.userId;
        entry.groupId = record.groupId;
        entry.size = record.size;
        entry.firstCluster = entry.kind == NandEntryKind::File ? record.firstClusterOrEntry : 0;
        entry.recordNumber = static_cast<std::uint16_t>(recordNumber);
        entry.unknownField = record.unknownField;
        return entry;
    }

    static NandEntryKind KindOf(const TreeRecord& record, std::size_t recordNumber)
    {
        std::uint8_t kindNumber = record.mode & KindMask;
        if (kindNumber == FileKindNumber)
            return NandEntryKind::File;

        if (kindNumber == DirectoryKindNumber)
            return NandEntryKind::Directory;

        throw FormatError(std::format("NAND file system: the record {} is neither a file nor a directory (kind {})",
            recordNumber, kindNumber));
    }

    const std::vector<TreeRecord>& records;
    std::vector<bool> isReached;
    std::vector<NandEntry> entries;
};

}

std::string_view RightsText(NandRights rights)
{
    constexpr std::string_view texts[] = {"--", "r-", "-w", "rw"};
    return texts[static_cast<std::size_t>(rights)];
}

std::string_view ParentPathOf(std::string_view path)
{
    std::size_t lastSeparator = path.rfind('/');
    if (lastSeparator == 0 || lastSeparator == std::string_view::npos)
        return RootPath;

    return path.substr(0, lastSeparator);
}

NandFileSystem::NandFileSystem(NandImage& nandImage, const AesKey& key)
    : image(nandImage)
    , nandKey(key)
{
    Superblock superblock = ReadNewestSuperblock(image);
    superblockFirstCluster = superblock.firstCluster;
    generation = superblock.generation;
    clusterChains = ReadClusterChains(superblock.bytes);

    std::vector<TreeRecord> records = ReadTreeRecords(superblock.bytes);
    entries = TreeBuilder(records).Build();

    for (std::size_t entryNumber = 0; entryNumber < entries.size(); ++entryNumber)
        entryNumberByPath.emplace(entries[entryNumber].path, entryNumber);
}

std::uint32_t NandFileSystem::Generation() const
{
    return generation;
}

const std::vector<NandEntry>& NandFileSystem::Entries() const
{
    return entries;
}

const NandEntry* NandFileSystem::Find(std::string_view path) const
{
    auto found = entryNumberByPath.find(std::string(path));
    if (found == entryNumberByPath.end())
        return nullptr;

    return &entries[found->second];
}

std::vector<const NandEntry*> NandFileSystem::ListDirectory(std::string_view directoryPath) const
{
    const NandEntry* directory = Find(directoryPath);
    if (directory == nullptr || directory->kind != NandEntryKind::Directory)
        throw FormatError(std::format("NAND file system: {} is not a directory", directoryPath));

    std::vector<const NandEntry*> children;
    for (const NandEntry& entry : entries)
    {
        bool isRoot = entry.path == RootPath;
        if (!isRoot && ParentPathOf(entry.path) == directory->path)
            children.push_back(&entry);
    }
    return children;
}

Bytes NandFileSystem::ReadFile(const NandEntry& file)
{
    if (file.kind != NandEntryKind::File)
        throw FormatError(std::format("NAND file system: {} is a directory, not a file", file.path));

    Bytes content;
    content.reserve(file.size);

    // The chain of a file starts at its first cluster. Each entry of the table names the next cluster.
    std::uint32_t cluster = file.firstCluster;
    while (content.size() < file.size)
    {
        if (cluster < FirstDataCluster || cluster >= DataClusterLimit)
        {
            throw FormatError(std::format("NAND file system: the clusters of {} end after {} of {} bytes, at the cluster {:#x}",
                file.path, content.size(), file.size, cluster));
        }

        Bytes clusterData = ReadDecryptedCluster(cluster);
        std::size_t usedSize = std::min<std::size_t>(clusterData.size(), file.size - content.size());
        content.insert(content.end(), clusterData.begin(), clusterData.begin() + usedSize);

        cluster = clusterChains[cluster];
    }
    return content;
}

Bytes NandFileSystem::ReadFile(std::string_view path)
{
    const NandEntry* entry = Find(path);
    if (entry == nullptr)
        throw FormatError(std::format("NAND file system: {} does not exist", path));

    return ReadFile(*entry);
}

// Each cluster of a file is encrypted on its own, with the key of the console and an initialization vector of zeros.
Bytes NandFileSystem::ReadDecryptedCluster(std::uint32_t clusterNumber)
{
    constexpr AesBlock ZeroInitializationVector = {};
    return DecryptAes128Cbc(image.ReadClusterData(clusterNumber), nandKey, ZeroInitializationVector);
}

std::size_t NandFileSystem::CountClustersWithWrongHmac(const NandEntry& file, ByteView hmacKey)
{
    if (file.kind != NandEntryKind::File)
        throw FormatError(std::format("NAND file system: {} is a directory, not a file", file.path));

    std::uint32_t clusterCount = (file.size + NandClusterSize - 1) / NandClusterSize;
    std::size_t wrongCount = 0;

    std::uint32_t cluster = file.firstCluster;
    for (std::uint32_t clusterInFile = 0; clusterInFile < clusterCount; ++clusterInFile)
    {
        if (cluster < FirstDataCluster || cluster >= DataClusterLimit)
        {
            throw FormatError(std::format("NAND file system: the clusters of {} end after {} of {} clusters, at the cluster {:#x}",
                file.path, clusterInFile, clusterCount, cluster));
        }

        Bytes salt = MakeHmacSalt(file.userId, file.name, clusterInFile, file.recordNumber, file.unknownField);
        Sha1Digest hmac = ComputeHmacSha1(hmacKey, JoinBytes(salt, ReadDecryptedCluster(cluster)));
        if (!StoredHmacMatches(cluster, hmac))
            ++wrongCount;

        cluster = clusterChains[cluster];
    }
    return wrongCount;
}

bool NandFileSystem::SuperblockHmacIsCorrect(ByteView hmacKey)
{
    Bytes superblockData;
    for (std::uint32_t clusterInSuperblock = 0; clusterInSuperblock < ClustersPerSuperblock; ++clusterInSuperblock)
    {
        Bytes clusterData = image.ReadClusterData(superblockFirstCluster + clusterInSuperblock);
        superblockData.insert(superblockData.end(), clusterData.begin(), clusterData.end());
    }

    Bytes salt = MakeHmacSalt(0, std::string(), superblockFirstCluster, 0, {});
    Sha1Digest hmac = ComputeHmacSha1(hmacKey, JoinBytes(salt, superblockData));

    std::uint32_t lastCluster = superblockFirstCluster + ClustersPerSuperblock - 1;
    return StoredHmacMatches(lastCluster, hmac);
}

bool NandFileSystem::StoredHmacMatches(std::uint32_t clusterNumber, const Sha1Digest& hmac)
{
    std::uint32_t hmacPage = clusterNumber * NandPagesPerCluster + HmacPageInCluster;
    Bytes firstPageSpare = image.ReadPageSpare(hmacPage);
    Bytes nextPageSpare = image.ReadPageSpare(hmacPage + 1);

    Bytes firstCopy(firstPageSpare.begin() + FirstCopyOffset, firstPageSpare.begin() + FirstCopyOffset + Sha1DigestSize);

    constexpr std::size_t secondCopySizeInNextPage = Sha1DigestSize - SecondCopySizeInFirstPage;
    Bytes secondCopy(firstPageSpare.begin() + SecondCopyOffset, firstPageSpare.begin() + SecondCopyOffset + SecondCopySizeInFirstPage);
    secondCopy.insert(secondCopy.end(), nextPageSpare.begin() + SecondCopyOffsetInNextPage,
        nextPageSpare.begin() + SecondCopyOffsetInNextPage + secondCopySizeInNextPage);

    bool firstCopyFits = std::equal(hmac.begin(), hmac.end(), firstCopy.begin());
    bool secondCopyFits = std::equal(hmac.begin(), hmac.end(), secondCopy.begin());
    return firstCopyFits && secondCopyFits;
}

}
