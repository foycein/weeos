#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/NandImage.h"
#include "Formats/Sha1.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Formats
{

enum class NandEntryKind : std::uint8_t
{
    File,
    Directory,
};

// What the owner, the group and everybody else may do with an entry.
enum class NandRights : std::uint8_t
{
    None = 0,
    Read = 1,
    Write = 2,
    ReadWrite = 3,
};

// Two letters like "rw", "r-", "-w" or "--", as a list of files shows rights.
std::string_view RightsText(NandRights rights);

// The last four bytes of a tree record are not known. They only go into the HMAC of the clusters.
constexpr std::size_t NandUnknownFieldSize = 4;

// The directory that holds an entry, for example "/title" for "/title/00000001". The parent of the root is the root.
std::string_view ParentPathOf(std::string_view path);

// One file or directory of the NAND file system.
struct NandEntry
{
    // For example "/title/00000001/00000002/content". The root directory is "/".
    std::string path;

    // The last part of the path. It has at most 12 characters. The root directory is "/".
    std::string name;

    NandEntryKind kind;
    NandRights ownerRights;
    NandRights groupRights;
    NandRights otherRights;

    // A byte the file system keeps for the programs and never looks at.
    std::uint8_t attributes;

    std::uint32_t userId;
    std::uint16_t groupId;

    // Files only: the size in bytes and the cluster where the data starts.
    std::uint32_t size;
    std::uint16_t firstCluster;

    // The number of the record of the tree that describes the entry, and the four bytes at the end of that record, whose meaning
    // nobody knows. Both go into the HMAC of the clusters of a file (see CountClustersWithWrongHmac).
    std::uint16_t recordNumber;
    std::array<Byte, NandUnknownFieldSize> unknownField;
};

// The file system of the NAND (SFFS), read from its newest superblock.
// The superblock holds a table that chains the clusters of every file and the tree of all files and directories.
// The file system keeps a reference to the image, which must outlive it.
class NandFileSystem
{
public:
    // Throws a FormatError when the image has no superblock or the tree in it is damaged.
    NandFileSystem(NandImage& nandImage, const AesKey& nandKey);

    // The number of the superblock. It grows with every change of the file system.
    std::uint32_t Generation() const;

    // The root directory first, then everything below it: a directory comes before its entries,
    // and the entries of a directory are in the order of the file system.
    const std::vector<NandEntry>& Entries() const;

    // The entry at a path like "/title/00000001", or nothing.
    const NandEntry* Find(std::string_view path) const;

    // The entries directly inside a directory. Throws a FormatError when the path is no directory.
    std::vector<const NandEntry*> ListDirectory(std::string_view directoryPath) const;

    // The decrypted data of a file. Throws a FormatError when the entry is no file or the chain of its clusters is damaged.
    Bytes ReadFile(const NandEntry& file);
    Bytes ReadFile(std::string_view path);

    // Some bytes of a file: only the clusters that hold them are decrypted. Throws a FormatError when the entry is no file, when the range
    // ends behind the end of the file or when the chain of its clusters is damaged.
    Bytes ReadFileRange(const NandEntry& file, std::uint64_t offset, std::size_t size);

    // An optional check of the HMAC that guards every cluster of a file against changes. The HMAC (SHA-1, made with the HMAC key of
    // the console) covers 64 bytes and then the decrypted data of the cluster. The 64 bytes are the owner and the name of the file,
    // the place of the cluster in the file, the number of the tree record and its unknown field. The spare bytes of the cluster hold
    // the HMAC twice. Gives the number of clusters of the file where one of the two copies is not the HMAC of the cluster: 0 is good.
    // Throws a FormatError when the chain of the clusters of the file is damaged.
    std::size_t CountClustersWithWrongHmac(const NandEntry& file, ByteView hmacKey);

    // The same check for the superblock this file system was read from. One HMAC covers its 16 clusters and is kept with the last one.
    bool SuperblockHmacIsCorrect(ByteView hmacKey);

private:
    Bytes ReadDecryptedCluster(std::uint32_t clusterNumber);
    void RequireDataCluster(const NandEntry& file, std::uint32_t clusterNumber, std::uint64_t bytesBefore) const;
    bool StoredHmacMatches(std::uint32_t clusterNumber, const Sha1Digest& hmac);

    NandImage& image;
    AesKey nandKey;
    std::uint32_t generation;
    std::uint32_t superblockFirstCluster;
    std::vector<std::uint16_t> clusterChains;
    std::vector<NandEntry> entries;
    std::unordered_map<std::string, std::size_t> entryNumberByPath;
};

}
