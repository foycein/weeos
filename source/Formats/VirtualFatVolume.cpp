#include "Formats/VirtualFatVolume.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <array>
#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view Magic = "VFF ";
constexpr std::uint16_t ByteOrderMark = 0xFEFF;
constexpr std::uint16_t Version = 0x0100;
constexpr std::size_t HeaderSize = 0x20;

constexpr std::size_t ClusterSize = 0x200;
constexpr std::size_t ClusterSizeUnit = 16;
constexpr std::size_t RootFolderSize = 0x1000;
constexpr std::size_t EntrySize = 32;
constexpr std::size_t EntriesPerCluster = ClusterSize / EntrySize;
constexpr std::size_t RootFolderEntryCount = RootFolderSize / EntrySize;

// FAT16 is the kind of FAT for 4085 to 65524 clusters.
constexpr std::size_t SmallestFat16ClusterCount = 4085;
constexpr std::size_t LargestFat16ClusterCount = 65524;

constexpr std::uint16_t FirstDataCluster = 2;
constexpr std::uint16_t FreeCluster = 0;
constexpr std::uint16_t MediaFatEntry = 0xFFF0;
constexpr std::uint16_t EndOfChain = 0xFFFF;
constexpr std::uint16_t SmallestEndOfChain = 0xFFF8;

constexpr Byte EndOfFolderMark = 0x00;
constexpr Byte DeletedEntryMark = 0xE5;

constexpr Byte FolderAttribute = 0x10;
constexpr Byte FileAttribute = 0x20;
constexpr Byte VolumeLabelAttribute = 0x08;
constexpr Byte LongNameAttribute = 0x0F;

constexpr std::size_t ShortNameSize = 11;
constexpr std::size_t ShortBaseSize = 8;
constexpr std::size_t ShortExtensionSize = 3;
constexpr std::size_t LongNameCharactersPerEntry = 13;
constexpr Byte LastLongNameEntryFlag = 0x40;
constexpr Byte LongNameSequenceMask = 0x1F;
constexpr char16_t LongNamePadding = 0xFFFF;

// The places of the 13 characters inside a long name entry.
constexpr std::array<std::size_t, LongNameCharactersPerEntry> LongNameCharacterOffsets = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

constexpr std::size_t AttributeOffset = 11;
constexpr std::size_t LongNameChecksumOffset = 13;
constexpr std::size_t CreationTimeOffset = 14;
constexpr std::size_t CreationDateOffset = 16;
constexpr std::size_t AccessDateOffset = 18;
constexpr std::size_t WriteTimeOffset = 22;
constexpr std::size_t WriteDateOffset = 24;
constexpr std::size_t FirstClusterOffset = 26;
constexpr std::size_t FileSizeOffset = 28;

constexpr int FatYearOrigin = 1980;
constexpr int SecondsPerTimeUnit = 2;

constexpr std::string_view ShortNameSymbols = "!#$%&'()-@^_`{}~";

std::uint16_t ReadLittleEndian16(const Bytes& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << BitsPerByte));
}

std::uint32_t ReadLittleEndian32(const Bytes& bytes, std::size_t offset)
{
    std::uint32_t lowHalf = ReadLittleEndian16(bytes, offset);
    std::uint32_t highHalf = ReadLittleEndian16(bytes, offset + 2);
    return lowHalf | (highHalf << 16);
}

void WriteLittleEndian16(Bytes& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<Byte>(value & ByteMask);
    bytes[offset + 1] = static_cast<Byte>(value >> BitsPerByte);
}

void WriteLittleEndian32(Bytes& bytes, std::size_t offset, std::uint32_t value)
{
    WriteLittleEndian16(bytes, offset, static_cast<std::uint16_t>(value & 0xFFFF));
    WriteLittleEndian16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16));
}

std::size_t AlignUp(std::size_t value, std::size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

std::uint16_t FatDate(const VolumeTime& time)
{
    return static_cast<std::uint16_t>(((time.year - FatYearOrigin) << 9) | (time.month << 5) | time.day);
}

std::uint16_t FatTime(const VolumeTime& time)
{
    return static_cast<std::uint16_t>((time.hour << 11) | (time.minute << 5) | (time.second / SecondsPerTimeUnit));
}

char ToUpper(char character)
{
    if (character >= 'a' && character <= 'z')
        return static_cast<char>(character - 'a' + 'A');
    return character;
}

bool IsShortNameCharacter(char character)
{
    bool isLetter = character >= 'A' && character <= 'Z';
    bool isDigit = character >= '0' && character <= '9';
    return isLetter || isDigit || ShortNameSymbols.find(character) != std::string_view::npos;
}

bool NamesAreEqual(std::string_view first, std::string_view second)
{
    if (first.size() != second.size())
        return false;

    for (std::size_t index = 0; index < first.size(); ++index)
    {
        if (ToUpper(first[index]) != ToUpper(second[index]))
            return false;
    }
    return true;
}

std::vector<std::string_view> SplitPath(std::string_view path)
{
    std::vector<std::string_view> parts;
    while (!path.empty())
    {
        std::size_t slash = path.find('/');
        std::string_view part = path.substr(0, slash);
        if (!part.empty())
            parts.push_back(part);
        if (slash == std::string_view::npos)
            break;
        path.remove_prefix(slash + 1);
    }
    return parts;
}

std::string_view ParentOf(std::string_view path)
{
    std::size_t slash = path.rfind('/');
    if (slash == std::string_view::npos)
        return {};
    return path.substr(0, slash);
}

std::string_view LastPartOf(std::string_view path)
{
    std::size_t slash = path.rfind('/');
    if (slash == std::string_view::npos)
        return path;
    return path.substr(slash + 1);
}

// "LOG        " is the folder "LOG", "2CE2D3A2000" the file "2CE2D3A2.000".
std::string ReadableShortName(const Bytes& bytes, std::size_t entryOffset)
{
    std::string base(reinterpret_cast<const char*>(&bytes[entryOffset]), ShortBaseSize);
    std::string extension(reinterpret_cast<const char*>(&bytes[entryOffset + ShortBaseSize]), ShortExtensionSize);
    base.erase(base.find_last_not_of(' ') + 1);
    extension.erase(extension.find_last_not_of(' ') + 1);
    if (extension.empty())
        return base;
    return base + "." + extension;
}

Byte ShortNameChecksum(const Bytes& bytes, std::size_t entryOffset)
{
    Byte sum = 0;
    for (std::size_t index = 0; index < ShortNameSize; ++index)
        sum = static_cast<Byte>(((sum & 1) << 7) + (sum >> 1) + bytes[entryOffset + index]);
    return sum;
}

// The part of a long name that one long name entry holds.
std::string ReadLongNamePart(const Bytes& bytes, std::size_t entryOffset)
{
    std::string part;
    for (std::size_t characterOffset : LongNameCharacterOffsets)
    {
        char16_t character = ReadLittleEndian16(bytes, entryOffset + characterOffset);
        if (character == 0 || character == LongNamePadding)
            break;
        part.push_back(character < 0x80 ? static_cast<char>(character) : '?');
    }
    return part;
}

}

VirtualFatVolume::VirtualFatVolume(Bytes fileBytes)
    : bytes(std::move(fileBytes))
{
}

VirtualFatVolume VirtualFatVolume::MakeEmpty(std::uint32_t volumeSize)
{
    ByteWriter header;
    header.WriteBytes(ByteView(reinterpret_cast<const Byte*>(Magic.data()), Magic.size()));
    header.WriteUint16(ByteOrderMark);
    header.WriteUint16(Version);
    header.WriteUint32(volumeSize);
    header.WriteUint16(static_cast<std::uint16_t>(ClusterSize / ClusterSizeUnit));
    header.WriteZeros(HeaderSize - header.GetBytes().size());

    Bytes fileBytes(volumeSize, 0);
    std::copy(header.GetBytes().begin(), header.GetBytes().end(), fileBytes.begin());

    VirtualFatVolume volume(std::move(fileBytes));
    volume.FindTheParts();
    volume.SetFatEntry(0, MediaFatEntry);
    volume.SetFatEntry(1, EndOfChain);
    return volume;
}

VirtualFatVolume VirtualFatVolume::Read(Bytes fileBytes)
{
    ByteReader header(fileBytes, "VFF header");
    std::string magic = header.ReadText(Magic.size());
    if (magic != Magic)
        header.Fail("the file does not start with \"VFF \"");
    if (header.ReadUint16() != ByteOrderMark)
        header.Fail("the byte order mark is not FE FF");

    header.Skip(sizeof(Version));
    std::uint32_t volumeSize = header.ReadUint32();
    if (volumeSize != fileBytes.size())
        header.Fail(std::format("the header says {} bytes, the file has {}", volumeSize, fileBytes.size()));
    if (header.ReadUint16() * ClusterSizeUnit != ClusterSize)
        header.Fail("the clusters are not 0x200 bytes");

    VirtualFatVolume volume(std::move(fileBytes));
    volume.FindTheParts();
    return volume;
}

void VirtualFatVolume::FindTheParts()
{
    std::size_t clustersOfTheSize = bytes.size() / ClusterSize;
    if (clustersOfTheSize < SmallestFat16ClusterCount || clustersOfTheSize > LargestFat16ClusterCount)
        throw FormatError(std::format("VFF: {} bytes do not make a FAT16 volume", bytes.size()));

    fatSize = AlignUp(clustersOfTheSize * sizeof(std::uint16_t), ClusterSize);
    firstFatOffset = HeaderSize;
    rootFolderOffset = firstFatOffset + 2 * fatSize;
    firstClusterOffset = rootFolderOffset + RootFolderSize;
    clusterCount = static_cast<std::uint16_t>((bytes.size() - firstClusterOffset) / ClusterSize);
}

const Bytes& VirtualFatVolume::FileBytes() const
{
    return bytes;
}

std::vector<VolumeEntry> VirtualFatVolume::ListFolder(std::string_view folderPath) const
{
    std::optional<std::uint16_t> folderCluster = FindFolderCluster(folderPath);
    if (!folderCluster)
        return {};

    std::vector<VolumeEntry> entries;
    for (const FoundEntry& found : ReadFolder(*folderCluster))
        entries.push_back(VolumeEntry{found.name, found.isFolder, found.size});
    return entries;
}

std::optional<Bytes> VirtualFatVolume::ReadFile(std::string_view filePath) const
{
    std::optional<FoundEntry> found = FindEntry(filePath);
    if (!found || found->isFolder)
        return std::nullopt;

    Bytes contents;
    contents.reserve(found->size);
    for (std::uint16_t cluster : ClusterChain(found->firstCluster))
    {
        std::size_t offset = OffsetOfCluster(cluster);
        std::size_t count = std::min(ClusterSize, found->size - contents.size());
        contents.insert(contents.end(), bytes.begin() + offset, bytes.begin() + offset + count);
        if (contents.size() == found->size)
            break;
    }
    if (contents.size() != found->size)
        throw FormatError(std::format("VFF: the clusters of {} end before its {} bytes", filePath, found->size));
    return contents;
}

void VirtualFatVolume::WriteFile(std::string_view filePath, ByteView contents, const VolumeTime& time)
{
    std::size_t clustersNeeded = AlignUp(contents.size(), ClusterSize) / ClusterSize;
    std::optional<FoundEntry> existing = FindEntry(filePath);
    if (existing && existing->isFolder)
        throw FormatError(std::format("VFF: {} is a folder", filePath));

    if (existing)
    {
        std::size_t clustersFreedFirst = ClusterChain(existing->firstCluster).size();
        if (FreeBytes() / ClusterSize + clustersFreedFirst < clustersNeeded)
            throw FormatError(std::format("VFF: no room for the {} bytes of {}", contents.size(), filePath));

        FreeChain(existing->firstCluster);
        std::uint16_t firstCluster = CopyIntoNewChain(contents);
        std::size_t entryOffset = OffsetOfEntry(existing->place.folderCluster, existing->place.entryIndex);
        WriteLittleEndian16(bytes, entryOffset + FirstClusterOffset, firstCluster);
        WriteLittleEndian32(bytes, entryOffset + FileSizeOffset, static_cast<std::uint32_t>(contents.size()));
        WriteLittleEndian16(bytes, entryOffset + WriteTimeOffset, FatTime(time));
        WriteLittleEndian16(bytes, entryOffset + WriteDateOffset, FatDate(time));
        WriteLittleEndian16(bytes, entryOffset + AccessDateOffset, FatDate(time));
        return;
    }

    std::uint16_t folderCluster = MakeFolders(ParentOf(filePath), time);

    // One cluster more, in case the folder has to grow for the new entry.
    if (FreeBytes() / ClusterSize < clustersNeeded + 1)
        throw FormatError(std::format("VFF: no room for the {} bytes of {}", contents.size(), filePath));

    std::uint16_t firstCluster = CopyIntoNewChain(contents);
    AddEntry(folderCluster, LastPartOf(filePath), false, firstCluster, static_cast<std::uint32_t>(contents.size()), time);
}

std::uint16_t VirtualFatVolume::CopyIntoNewChain(ByteView contents)
{
    std::size_t clustersNeeded = AlignUp(contents.size(), ClusterSize) / ClusterSize;
    std::uint16_t firstCluster = AllocateChain(clustersNeeded);
    std::size_t written = 0;
    for (std::uint16_t cluster : ClusterChain(firstCluster))
    {
        std::size_t offset = OffsetOfCluster(cluster);
        std::size_t count = std::min(ClusterSize, contents.size() - written);
        std::fill(bytes.begin() + offset, bytes.begin() + offset + ClusterSize, Byte{0});
        std::copy(contents.begin() + written, contents.begin() + written + count, bytes.begin() + offset);
        written += count;
    }
    return firstCluster;
}

bool VirtualFatVolume::DeleteFile(std::string_view filePath)
{
    std::optional<FoundEntry> found = FindEntry(filePath);
    if (!found || found->isFolder)
        return false;

    FreeChain(found->firstCluster);
    MarkEntryDeleted(found->place);
    return true;
}

bool VirtualFatVolume::DeleteFolder(std::string_view folderPath)
{
    std::optional<FoundEntry> found = FindEntry(folderPath);
    if (!found || !found->isFolder)
        return false;
    if (!ReadFolder(found->firstCluster).empty())
        return false;

    FreeChain(found->firstCluster);
    MarkEntryDeleted(found->place);
    return true;
}

std::uint64_t VirtualFatVolume::FreeBytes() const
{
    std::uint64_t freeClusters = 0;
    for (std::uint32_t cluster = FirstDataCluster; cluster < EndOfClusters(); ++cluster)
    {
        if (FatEntry(static_cast<std::uint16_t>(cluster)) == FreeCluster)
            ++freeClusters;
    }
    return freeClusters * ClusterSize;
}

std::vector<VirtualFatVolume::FoundEntry> VirtualFatVolume::ReadFolder(std::uint16_t folderCluster) const
{
    std::vector<FoundEntry> entries;
    std::string longName;
    std::size_t longNameEntryCount = 0;
    std::size_t entryCount = EntryCountOfFolder(folderCluster);
    for (std::size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex)
    {
        std::size_t offset = OffsetOfEntry(folderCluster, entryIndex);
        Byte firstByte = bytes[offset];
        if (firstByte == EndOfFolderMark)
            break;

        Byte attribute = bytes[offset + AttributeOffset];
        if (firstByte == DeletedEntryMark)
        {
            longName.clear();
            longNameEntryCount = 0;
            continue;
        }

        if (attribute == LongNameAttribute)
        {
            longName = ReadLongNamePart(bytes, offset) + longName;
            ++longNameEntryCount;
            continue;
        }

        std::string shortName = ReadableShortName(bytes, offset);
        bool isSpecial = shortName == "." || shortName == ".." || (attribute & VolumeLabelAttribute) != 0;
        if (!isSpecial)
        {
            FoundEntry entry;
            entry.place = EntryPlace{folderCluster, entryIndex, longNameEntryCount};
            entry.name = longName.empty() ? shortName : longName;
            entry.isFolder = (attribute & FolderAttribute) != 0;
            entry.firstCluster = ReadLittleEndian16(bytes, offset + FirstClusterOffset);
            entry.size = ReadLittleEndian32(bytes, offset + FileSizeOffset);
            entries.push_back(entry);
        }
        longName.clear();
        longNameEntryCount = 0;
    }
    return entries;
}

std::optional<VirtualFatVolume::FoundEntry> VirtualFatVolume::FindEntry(std::string_view path) const
{
    std::vector<std::string_view> parts = SplitPath(path);
    if (parts.empty())
        return std::nullopt;

    std::uint16_t folderCluster = 0;
    for (std::size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
    {
        std::vector<FoundEntry> entries = ReadFolder(folderCluster);
        auto match = std::find_if(entries.begin(), entries.end(), [&](const FoundEntry& entry) {
            return NamesAreEqual(entry.name, parts[partIndex]);
        });
        if (match == entries.end())
            return std::nullopt;

        bool isLastPart = partIndex + 1 == parts.size();
        if (isLastPart)
            return *match;
        if (!match->isFolder)
            return std::nullopt;
        folderCluster = match->firstCluster;
    }
    return std::nullopt;
}

std::optional<std::uint16_t> VirtualFatVolume::FindFolderCluster(std::string_view folderPath) const
{
    if (SplitPath(folderPath).empty())
        return std::uint16_t{0};

    std::optional<FoundEntry> found = FindEntry(folderPath);
    if (!found || !found->isFolder)
        return std::nullopt;
    return found->firstCluster;
}

std::uint16_t VirtualFatVolume::MakeFolders(std::string_view folderPath, const VolumeTime& time)
{
    std::uint16_t folderCluster = 0;
    for (std::string_view part : SplitPath(folderPath))
    {
        std::vector<FoundEntry> entries = ReadFolder(folderCluster);
        auto match = std::find_if(entries.begin(), entries.end(), [&](const FoundEntry& entry) {
            return NamesAreEqual(entry.name, part);
        });
        if (match != entries.end())
        {
            if (!match->isFolder)
                throw FormatError(std::format("VFF: {} is a file, not a folder", part));
            folderCluster = match->firstCluster;
            continue;
        }

        if (FreeBytes() < ClusterSize)
            throw FormatError(std::format("VFF: no room for the folder {}", part));

        std::uint16_t newCluster = AllocateChain(1);
        std::size_t offset = OffsetOfCluster(newCluster);
        std::fill(bytes.begin() + offset, bytes.begin() + offset + ClusterSize, Byte{0});
        AddEntry(newCluster, ".", true, newCluster, 0, time);
        AddEntry(newCluster, "..", true, folderCluster, 0, time);
        AddEntry(folderCluster, part, true, newCluster, 0, time);
        folderCluster = newCluster;
    }
    return folderCluster;
}

std::size_t VirtualFatVolume::EntryCountOfFolder(std::uint16_t folderCluster) const
{
    if (folderCluster == 0)
        return RootFolderEntryCount;
    return ClusterChain(folderCluster).size() * EntriesPerCluster;
}

std::size_t VirtualFatVolume::OffsetOfEntry(std::uint16_t folderCluster, std::size_t entryIndex) const
{
    if (folderCluster == 0)
        return rootFolderOffset + entryIndex * EntrySize;

    std::vector<std::uint16_t> chain = ClusterChain(folderCluster);
    std::uint16_t cluster = chain.at(entryIndex / EntriesPerCluster);
    return OffsetOfCluster(cluster) + (entryIndex % EntriesPerCluster) * EntrySize;
}

// The first run of free entries that is long enough. A folder other than the root grows by a cluster when it has none.
std::size_t VirtualFatVolume::FindFreeEntries(std::uint16_t folderCluster, std::size_t count)
{
    while (true)
    {
        std::size_t entryCount = EntryCountOfFolder(folderCluster);
        std::size_t runLength = 0;
        for (std::size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex)
        {
            Byte firstByte = bytes[OffsetOfEntry(folderCluster, entryIndex)];
            bool isFree = firstByte == EndOfFolderMark || firstByte == DeletedEntryMark;
            runLength = isFree ? runLength + 1 : 0;
            if (runLength == count)
                return entryIndex + 1 - count;
        }

        if (folderCluster == 0)
            throw FormatError("VFF: the root folder is full");
        if (FreeBytes() < ClusterSize)
            throw FormatError("VFF: no room for a larger folder");

        std::vector<std::uint16_t> chain = ClusterChain(folderCluster);
        std::uint16_t newCluster = AllocateChain(1);
        SetFatEntry(chain.back(), newCluster);
        std::size_t offset = OffsetOfCluster(newCluster);
        std::fill(bytes.begin() + offset, bytes.begin() + offset + ClusterSize, Byte{0});
    }
}

void VirtualFatVolume::AddEntry(std::uint16_t folderCluster, std::string_view name, bool isFolder, std::uint16_t firstCluster,
    std::uint32_t size, const VolumeTime& time)
{
    std::string shortName = MakeShortName(folderCluster, name);
    bool needsLongName = shortName != name;
    std::size_t longNameEntryCount = needsLongName ? AlignUp(name.size(), LongNameCharactersPerEntry) / LongNameCharactersPerEntry : 0;
    std::size_t firstIndex = FindFreeEntries(folderCluster, longNameEntryCount + 1);

    std::size_t shortEntryOffset = OffsetOfEntry(folderCluster, firstIndex + longNameEntryCount);
    std::fill(bytes.begin() + shortEntryOffset, bytes.begin() + shortEntryOffset + EntrySize, Byte{0});
    std::fill(bytes.begin() + shortEntryOffset, bytes.begin() + shortEntryOffset + ShortNameSize, Byte{' '});
    std::size_t dot = (shortName == "." || shortName == "..") ? std::string::npos : shortName.find('.');
    std::string base = shortName.substr(0, dot);
    std::string extension = dot == std::string::npos ? std::string() : shortName.substr(dot + 1);
    std::copy(base.begin(), base.end(), bytes.begin() + shortEntryOffset);
    std::copy(extension.begin(), extension.end(), bytes.begin() + shortEntryOffset + ShortBaseSize);

    bytes[shortEntryOffset + AttributeOffset] = isFolder ? FolderAttribute : FileAttribute;
    WriteLittleEndian16(bytes, shortEntryOffset + CreationTimeOffset, FatTime(time));
    WriteLittleEndian16(bytes, shortEntryOffset + CreationDateOffset, FatDate(time));
    WriteLittleEndian16(bytes, shortEntryOffset + AccessDateOffset, FatDate(time));
    WriteLittleEndian16(bytes, shortEntryOffset + WriteTimeOffset, FatTime(time));
    WriteLittleEndian16(bytes, shortEntryOffset + WriteDateOffset, FatDate(time));
    WriteLittleEndian16(bytes, shortEntryOffset + FirstClusterOffset, firstCluster);
    WriteLittleEndian32(bytes, shortEntryOffset + FileSizeOffset, size);

    Byte checksum = ShortNameChecksum(bytes, shortEntryOffset);
    for (std::size_t sequence = 1; sequence <= longNameEntryCount; ++sequence)
    {
        std::size_t entryOffset = OffsetOfEntry(folderCluster, firstIndex + longNameEntryCount - sequence);
        std::fill(bytes.begin() + entryOffset, bytes.begin() + entryOffset + EntrySize, Byte{0});
        bool isLast = sequence == longNameEntryCount;
        bytes[entryOffset] = static_cast<Byte>(isLast ? (sequence | LastLongNameEntryFlag) : sequence);
        bytes[entryOffset + AttributeOffset] = LongNameAttribute;
        bytes[entryOffset + LongNameChecksumOffset] = checksum;

        std::size_t firstCharacter = (sequence - 1) * LongNameCharactersPerEntry;
        for (std::size_t slot = 0; slot < LongNameCharactersPerEntry; ++slot)
        {
            std::size_t characterIndex = firstCharacter + slot;
            char16_t character = LongNamePadding;
            if (characterIndex < name.size())
                character = static_cast<char16_t>(name[characterIndex]);
            else if (characterIndex == name.size())
                character = 0;
            WriteLittleEndian16(bytes, entryOffset + LongNameCharacterOffsets[slot], character);
        }
    }
}

void VirtualFatVolume::MarkEntryDeleted(const EntryPlace& place)
{
    std::size_t firstIndex = place.entryIndex - place.longNameEntryCount;
    for (std::size_t entryIndex = firstIndex; entryIndex <= place.entryIndex; ++entryIndex)
        bytes[OffsetOfEntry(place.folderCluster, entryIndex)] = DeletedEntryMark;
}

// "2023" stays "2023"; "log" becomes "LOG" with a long name; "cdb.conf" becomes "CDB~1.CON" with a long name, as the VF library names them.
std::string VirtualFatVolume::MakeShortName(std::uint16_t folderCluster, std::string_view longName) const
{
    if (longName == "." || longName == "..")
        return std::string(longName);

    std::size_t dot = longName.rfind('.');
    if (dot == 0)
        dot = std::string_view::npos;
    std::string_view base = longName.substr(0, dot);
    std::string_view extension = dot == std::string_view::npos ? std::string_view() : longName.substr(dot + 1);

    std::string upperBase;
    std::string upperExtension;
    for (char character : base)
    {
        if (IsShortNameCharacter(ToUpper(character)))
            upperBase.push_back(ToUpper(character));
    }
    for (char character : extension)
    {
        if (IsShortNameCharacter(ToUpper(character)) && upperExtension.size() < ShortExtensionSize)
            upperExtension.push_back(ToUpper(character));
    }

    std::vector<FoundEntry> entries = ReadFolder(folderCluster);
    auto isTaken = [&](const std::string& candidate) {
        return std::any_of(entries.begin(), entries.end(), [&](const FoundEntry& entry) {
            return NamesAreEqual(ReadableShortName(bytes, OffsetOfEntry(folderCluster, entry.place.entryIndex)), candidate);
        });
    };
    auto joined = [](const std::string& shortBase, const std::string& shortExtension) {
        return shortExtension.empty() ? shortBase : shortBase + "." + shortExtension;
    };

    bool keepsEveryCharacter = upperBase.size() == base.size() && upperExtension.size() == extension.size();
    bool fits = !upperBase.empty() && upperBase.size() <= ShortBaseSize;
    std::string plainCandidate = joined(upperBase, upperExtension);
    if (keepsEveryCharacter && fits && !isTaken(plainCandidate))
        return plainCandidate;

    constexpr std::size_t LongestBasis = 6;
    std::string basis = upperBase.substr(0, LongestBasis);
    for (int number = 1;; ++number)
    {
        std::string tail = "~" + std::to_string(number);
        std::string candidate = joined(basis.substr(0, ShortBaseSize - tail.size()) + tail, upperExtension);
        if (!isTaken(candidate))
            return candidate;
    }
}

std::uint16_t VirtualFatVolume::FatEntry(std::uint16_t cluster) const
{
    return ReadLittleEndian16(bytes, firstFatOffset + cluster * sizeof(std::uint16_t));
}

void VirtualFatVolume::SetFatEntry(std::uint16_t cluster, std::uint16_t value)
{
    std::size_t place = cluster * sizeof(std::uint16_t);
    WriteLittleEndian16(bytes, firstFatOffset + place, value);
    WriteLittleEndian16(bytes, firstFatOffset + fatSize + place, value);
}

std::vector<std::uint16_t> VirtualFatVolume::ClusterChain(std::uint16_t firstCluster) const
{
    std::vector<std::uint16_t> chain;
    std::uint16_t cluster = firstCluster;
    while (cluster >= FirstDataCluster && cluster < EndOfClusters())
    {
        chain.push_back(cluster);
        if (chain.size() > clusterCount)
            throw FormatError("VFF: a chain of clusters runs in a circle");
        cluster = FatEntry(cluster);
    }
    bool endsProperly = chain.empty() || cluster >= SmallestEndOfChain;
    if (!endsProperly)
        throw FormatError(std::format("VFF: a chain of clusters leads to cluster {:#x}", cluster));
    return chain;
}

std::optional<std::uint16_t> VirtualFatVolume::FindFreeCluster() const
{
    for (std::uint32_t cluster = FirstDataCluster; cluster < EndOfClusters(); ++cluster)
    {
        if (FatEntry(static_cast<std::uint16_t>(cluster)) == FreeCluster)
            return static_cast<std::uint16_t>(cluster);
    }
    return std::nullopt;
}

std::uint16_t VirtualFatVolume::AllocateChain(std::size_t chainLength)
{
    std::uint16_t firstCluster = 0;
    std::uint16_t previousCluster = 0;
    for (std::size_t index = 0; index < chainLength; ++index)
    {
        std::optional<std::uint16_t> cluster = FindFreeCluster();
        if (!cluster)
            throw FormatError("VFF: the volume is full");

        SetFatEntry(*cluster, EndOfChain);
        if (previousCluster == 0)
            firstCluster = *cluster;
        else
            SetFatEntry(previousCluster, *cluster);
        previousCluster = *cluster;
    }
    return firstCluster;
}

void VirtualFatVolume::FreeChain(std::uint16_t firstCluster)
{
    for (std::uint16_t cluster : ClusterChain(firstCluster))
        SetFatEntry(cluster, FreeCluster);
}

std::uint32_t VirtualFatVolume::EndOfClusters() const
{
    return std::uint32_t{FirstDataCluster} + clusterCount;
}

std::size_t VirtualFatVolume::OffsetOfCluster(std::uint16_t cluster) const
{
    return firstClusterOffset + (cluster - FirstDataCluster) * ClusterSize;
}

}
