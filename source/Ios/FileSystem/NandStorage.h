#pragma once

#include "Ios/FileSystem/EntryIndexFile.h"
#include "Ios/FileSystem/EntryRecord.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/Result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Ios
{

// How much of the NAND a directory takes: whole clusters for the data of the files in it,
// and one inode for the directory itself and for every entry in it.
struct DirectoryUsage
{
    std::uint32_t usedClusters;
    std::uint32_t usedInodes;
};

// The NAND on the PC. It is a folder with one file or folder for every NAND file or directory, under the same names and in the same layout,
// and the index file that keeps what a file on the PC cannot keep: the owner, the group, the permissions and the flags.
//
// The storage only stores. It does not decide who may do what, and it does not count limits: that is the file system service.
// It is not safe to use from several threads at once.
//
// When the index cannot be written the call gives back StorageFailure, but the change stays, so the storage and the folder keep agreeing.
class NandStorage
{
public:
    // Opens the NAND in the folder. The folder is created when it is missing.
    // A folder without an index is a new, empty NAND. Entries of the index that are missing in the folder are dropped.
    // Gives back NoFileSystem when the index is damaged or the folder cannot be used.
    static Result<NandStorage> Open(const std::filesystem::path& nandFolder);

    // The record of an entry, or nothing when there is no such entry. The pointer is valid until the next change.
    const EntryRecord* Find(const NandPath& path) const;

    // The entries directly inside a directory, the newest first.
    std::vector<NandPath> ListChildren(const NandPath& directory) const;

    // The usage of a directory and everything inside it.
    DirectoryUsage MeasureUsage(const NandPath& directory) const;

    // Creates an empty file or directory. Its parent directory must exist.
    Result<void> Create(const NandPath& path, EntryKind kind, const EntryAttributes& attributes);

    // Deletes an entry and everything inside it.
    Result<void> Remove(const NandPath& path);

    // Moves an entry, with everything inside it, to another path. An entry of the same kind that is at the new path is replaced.
    // The moved entry becomes the newest one in its new directory.
    // InvalidArgument when the entry would move into itself or onto a directory that it lies in.
    // AlreadyExists when the entry at the new path is of another kind.
    Result<void> Move(const NandPath& from, const NandPath& to);

    Result<void> SetAttributes(const NandPath& path, const EntryAttributes& attributes);

    // Tells the storage that the data of a file has this size now. The file must exist.
    void SetLength(const NandPath& path, std::uint32_t length);

    // Where the data of a file is kept on the PC, or the folder of a directory. The entry must exist.
    std::filesystem::path HostPath(const NandPath& path) const;

private:
    NandStorage(std::filesystem::path folder, EntryMap loadedEntries);

    void DropEntriesMissingOnHost();
    bool IsPresentOnHost(const NandPath& path, EntryRecord& record) const;
    std::vector<std::pair<NandPath, EntryRecord>> EraseSubtree(const NandPath& path);
    std::vector<std::string> HostNamesInside(const NandPath& directory, const NandPath& ignoredEntry) const;
    Result<void> SaveIndex() const;

    std::filesystem::path nandFolder;
    EntryMap entries;
    std::uint64_t nextLinkNumber;
};

}
