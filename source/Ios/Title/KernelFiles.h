#pragma once

#include "Formats/Bytes.h"
#include "Ios/FileSystem/EntryPermissions.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/Result.h"

#include <string>
#include <vector>

namespace Ios
{

// The title service reads and writes the NAND as the kernel does: as the super user, whole files at a time.
// Entries it makes belong to the super user and can be read and written by the owner and the group, as the console makes them.
class KernelFiles
{
public:
    // The file system must outlive this object.
    explicit KernelFiles(FileSystemService& fileSystemToUse);

    bool Exists(const NandPath& path) const;

    Result<Formats::Bytes> ReadFile(const NandPath& path) const;

    // Replaces the file with the bytes, or makes it. When the NAND has no room for them the file is not there afterwards.
    Result<void> WriteFile(const NandPath& path, Formats::ByteView bytes);

    // The names inside a directory, the newest first.
    Result<std::vector<std::string>> ListNames(const NandPath& directory) const;

    // Makes the directory and gives it these permissions. Gives back success when it is there already.
    Result<void> EnsureDirectory(const NandPath& path, const EntryPermissions& permissions);

    // Deletes a file, or a directory with everything inside it.
    Result<void> Delete(const NandPath& path);

    // Gives back success when the entry is not there.
    Result<void> DeleteIfPresent(const NandPath& path);

    Result<void> Rename(const NandPath& from, const NandPath& to);

    Result<void> SetOwner(const NandPath& path, std::uint32_t userId, std::uint16_t groupId);

private:
    FileSystemService& fileSystem;
};

constexpr EntryPermissions SystemEntryPermissions = {AccessRights::ReadWrite, AccessRights::ReadWrite, AccessRights::None};
constexpr EntryPermissions TitleFolderPermissions = {AccessRights::ReadWrite, AccessRights::ReadWrite, AccessRights::Read};
constexpr EntryPermissions OwnerOnlyPermissions = {AccessRights::ReadWrite, AccessRights::None, AccessRights::None};

}
