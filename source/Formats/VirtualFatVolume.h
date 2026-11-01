#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Formats
{

// The date and the time that the file system writes into a new or changed entry.
struct VolumeTime
{
    int year = 2000;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

// A file or a folder of a folder.
struct VolumeEntry
{
    std::string name;
    bool isFolder = false;
    std::uint32_t size = 0;
};

// A VFF file: a FAT file system kept inside one file of the NAND, as the VF library of the System Menu keeps the Message Board
// in /title/00000001/00000002/data/cdb.vff (FAT16) and the Internet Channel its favorites and settings in opera.vff (FAT12). A header of 0x20 bytes ("VFF ", the byte order mark, the version, the size of the
// file, the cluster size in units of 16 bytes), two equal copies of the FAT, a root folder of 128 entries and clusters of 0x200 bytes.
// Names longer than 8.3 or with small letters get long name entries, as on any FAT volume.
// Paths are relative to the root and use '/', for example "2023/10/12/00/06/HAEA_01/txt/2CE2D3A2.000". Names are compared
// without regard to case.
class VirtualFatVolume
{
public:
    // A new empty volume of this many bytes: FAT12 below 4085 clusters, FAT16 above. Throws a FormatError for a size too large for FAT16.
    static VirtualFatVolume MakeEmpty(std::uint32_t volumeSize);

    // Throws a FormatError when the header or the size is wrong.
    static VirtualFatVolume Read(Bytes fileBytes);

    // The whole file, to be written back to the NAND.
    const Bytes& FileBytes() const;

    // The entries of a folder, in the order of the folder. Empty when the folder does not exist.
    std::vector<VolumeEntry> ListFolder(std::string_view folderPath) const;

    // Nothing when the file does not exist.
    std::optional<Bytes> ReadFile(std::string_view filePath) const;

    // Makes the file, or replaces the contents of the file that has this path. Makes the folders on the way.
    // Throws a FormatError when the volume is full.
    void WriteFile(std::string_view filePath, ByteView contents, const VolumeTime& time);

    // False when the file does not exist.
    bool DeleteFile(std::string_view filePath);

    // Deletes a folder that holds nothing. False when it does not exist or is not empty.
    bool DeleteFolder(std::string_view folderPath);

    std::uint64_t FreeBytes() const;

private:
    struct EntryPlace
    {
        // The cluster of the folder that holds the entry, 0 for the root folder.
        std::uint16_t folderCluster = 0;
        std::size_t entryIndex = 0;

        // The long name entries before it.
        std::size_t longNameEntryCount = 0;
    };

    struct FoundEntry
    {
        EntryPlace place;
        std::string name;
        bool isFolder = false;
        std::uint16_t firstCluster = 0;
        std::uint32_t size = 0;
    };

    explicit VirtualFatVolume(Bytes fileBytes);

    void FindTheParts();
    std::vector<FoundEntry> ReadFolder(std::uint16_t folderCluster) const;
    std::optional<FoundEntry> FindEntry(std::string_view path) const;
    std::optional<std::uint16_t> FindFolderCluster(std::string_view folderPath) const;
    std::uint16_t MakeFolders(std::string_view folderPath, const VolumeTime& time);

    std::size_t EntryCountOfFolder(std::uint16_t folderCluster) const;
    std::size_t OffsetOfEntry(std::uint16_t folderCluster, std::size_t entryIndex) const;
    std::size_t FindFreeEntries(std::uint16_t folderCluster, std::size_t count);
    void AddEntry(std::uint16_t folderCluster, std::string_view name, bool isFolder, std::uint16_t firstCluster, std::uint32_t size,
        const VolumeTime& time);
    void MarkEntryDeleted(const EntryPlace& place);
    std::string MakeShortName(std::uint16_t folderCluster, std::string_view longName) const;

    std::uint16_t FatEntry(std::uint16_t cluster) const;
    void SetFatEntry(std::uint16_t cluster, std::uint16_t value);
    std::vector<std::uint16_t> ClusterChain(std::uint16_t firstCluster) const;
    std::optional<std::uint16_t> FindFreeCluster() const;
    std::uint16_t AllocateChain(std::size_t chainLength);
    std::uint16_t CopyIntoNewChain(ByteView contents);
    void FreeChain(std::uint16_t firstCluster);
    std::uint32_t EndOfClusters() const;
    std::size_t OffsetOfCluster(std::uint16_t cluster) const;

    Bytes bytes;
    bool isFat12 = false;
    std::size_t fatSize = 0;
    std::size_t firstFatOffset = 0;
    std::size_t rootFolderOffset = 0;
    std::size_t firstClusterOffset = 0;
    std::uint16_t clusterCount = 0;
};

}
