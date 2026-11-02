#pragma once

#include "Ios/Caller.h"
#include "Ios/FileSystem/EntryAttributes.h"
#include "Ios/FileSystem/NandLimits.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/FileSystem/NandStorage.h"
#include "Ios/FileSystem/OpenFile.h"
#include "Ios/Profile/ProfileChoice.h"
#include "Ios/Result.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Ios
{

struct FileSystemState;

// The numbers that the statistics call of the file system reports. Space is counted in clusters of ClusterSize bytes.
struct VolumeStatistics
{
    std::uint32_t clusterSize;
    std::uint32_t freeClusters;
    std::uint32_t usedClusters;
    std::uint32_t badClusters;
    std::uint32_t reservedClusters;
    std::uint32_t freeInodes;
    std::uint32_t usedInodes;

    bool operator==(const VolumeStatistics&) const = default;
};

// The file system of IOS: the whole NAND, kept as plain files and folders on the PC.
//
// It takes the place of /dev/fs and of every path outside /dev. It follows the file system module of IOS80:
// the same limits, the same permission checks in the same order, the same result codes. Every call says who asks.
// The caller with the user id 0 may do everything. Any other caller is held to the owner, the group and the others of each entry.
//
// Every call gives back ShutDown after Shutdown. Calls from several threads are safe.
class FileSystemService
{
public:
    // The file system of the System Menu's IOS (IOS80).
    explicit FileSystemService(NandStorage storage, const VolumeCapacity& capacity = ConsoleVolumeCapacity);

    // The file system of another IOS. The profile says how the look-up treats a name of 13 characters or more.
    FileSystemService(NandStorage storage, const IosProfile& profile, const VolumeCapacity& capacity = ConsoleVolumeCapacity);

    // The space that is used and free on the NAND.
    Result<VolumeStatistics> GetVolumeStatistics() const;

    // The clusters and inodes of a directory and everything inside it. The console does not ask who wants to know.
    // InvalidArgument for a file.
    Result<DirectoryUsage> GetDirectoryUsage(const NandPath& directory) const;

    // The new entry belongs to the caller. The caller needs the right to write in the directory that will hold it.
    // Directories may be 8 deep, a file may lie in the deepest one. A name has at most 12 characters.
    Result<void> CreateDirectory(const NandPath& path, const EntryPermissions& permissions, std::uint8_t flags, const Caller& caller);
    Result<void> CreateFile(const NandPath& path, const EntryPermissions& permissions, std::uint8_t flags, const Caller& caller);

    // The names in a directory, the newest entry first. The caller needs the right to read the directory.
    Result<std::vector<std::string>> ListDirectory(const NandPath& directory, const Caller& caller) const;

    // The caller needs the right to read the directory that holds the entry. Nobody is asked about the root.
    Result<EntryAttributes> GetAttributes(const NandPath& path, const Caller& caller) const;

    // Only the owner and the super user may change an entry. Only the super user may change the owner,
    // and not of a file that holds data (NotEmpty).
    Result<void> SetAttributes(const NandPath& path, const EntryAttributes& attributes, const Caller& caller);

    // The caller needs the right to write in the directory that holds the entry. A directory is deleted with everything inside it.
    // FileInUse when a file in it is open.
    Result<void> DeleteEntry(const NandPath& path, const Caller& caller);

    // The caller needs the right to write in both directories. A file keeps its name and moves to another directory,
    // a directory may get another name too. An entry of the same kind at the new path is replaced.
    // The moved entry becomes the newest in its directory. FileInUse when it or a file inside it is open.
    Result<void> RenameEntry(const NandPath& from, const NandPath& to, const Caller& caller);

    // Opens a file. The caller needs the rights of the mode on the file itself. At most 15 files can be open.
    Result<OpenFile> Open(const NandPath& path, OpenMode mode, const Caller& caller);

    // Ends the file system for good, as the console does when it powers down.
    // Every later call gives back ShutDown, also the calls on files that are open.
    void Shutdown();

private:
    std::shared_ptr<FileSystemState> state;
};

}
