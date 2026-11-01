#pragma once

#include "SystemMenu/DataManagement/DataResult.h"

#include "Formats/Bytes.h"
#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/SdCard/SdCardService.h"
#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The files that Data Management reads and writes: on the NAND as a given caller, and on the SD card.
// A step that fails throws a DataError with the result that the screens show for it.

// The result that the screens show when a call of IOS failed in the middle of the work. On the SD card a refused write means
// that the card is write protected.
DataResult ResultOfIosError(Ios::IosError error);
DataResult ResultOfCardError(Ios::IosError error);

template <typename ValueType>
ValueType Require(Ios::Result<ValueType> result, std::string_view whatWasDone)
{
    if (result.Failed())
        throw DataError(ResultOfIosError(result.Error()), std::string(whatWasDone));

    return std::move(result.Value());
}

void Require(Ios::Result<void> result, std::string_view whatWasDone);

template <typename ValueType>
ValueType RequireOnCard(Ios::Result<ValueType> result, std::string_view whatWasDone)
{
    if (result.Failed())
        throw DataError(ResultOfCardError(result.Error()), std::string(whatWasDone));

    return std::move(result.Value());
}

void RequireOnCard(Ios::Result<void> result, std::string_view whatWasDone);

Ios::NandPath MakeNandPath(std::string_view text);
Ios::NandPath ChildOf(const Ios::NandPath& directory, std::string_view name);

// The folder /meta/<high half>/<low half> where a channel keeps its meta data.
Ios::NandPath MetaFolderPath(Ios::TitleId titleId);

// The NAND, as one caller sees it.
class NandAccess
{
public:
    // The file system must outlive this object.
    NandAccess(Ios::FileSystemService& fileSystem, const Ios::Caller& caller);

    bool Exists(const Ios::NandPath& path) const;
    Ios::EntryKind KindOf(const Ios::NandPath& path) const;
    Ios::EntryAttributes AttributesOf(const Ios::NandPath& path) const;
    std::vector<std::string> List(const Ios::NandPath& directory) const;

    Formats::Bytes ReadFile(const Ios::NandPath& path) const;

    // The new file belongs to the caller.
    void WriteNewFile(const Ios::NandPath& path, Formats::ByteView bytes, const Ios::EntryPermissions& permissions, std::uint8_t flags);
    void CreateDirectory(const Ios::NandPath& path, const Ios::EntryPermissions& permissions, std::uint8_t flags);
    void SetPermissions(const Ios::NandPath& path, const Ios::EntryPermissions& permissions);

    // A directory goes with everything inside.
    void DeleteIfPresent(const Ios::NandPath& path);
    void Move(const Ios::NandPath& from, const Ios::NandPath& to);

private:
    Ios::FileSystemService& fileSystem;
    Ios::Caller caller;
};

// The rights of a NAND entry as the NAND library of the SDK keeps them in one byte, the way the backup files on the SD card store them:
// bit 5 the owner may write, bit 4 the owner may read, bits 3 and 2 the same for the group, bits 1 and 0 for everybody else.
std::uint8_t PermissionsToByte(const Ios::EntryPermissions& permissions);
Ios::EntryPermissions PermissionsFromByte(std::uint8_t permissionsByte);

// Files of the SD card.
Ios::SdPath MakeSdPath(std::string_view text);
std::optional<std::uint64_t> SdFileSize(const Ios::SdCardService& sdCard, const Ios::SdPath& path);
Formats::Bytes ReadSdFile(Ios::SdCardService& sdCard, const Ios::SdPath& path);

// The first bytes of a file, fewer when the file is shorter.
Formats::Bytes ReadSdFileStart(Ios::SdCardService& sdCard, const Ios::SdPath& path, std::size_t count);

// Makes the file, or replaces the one that is there.
void WriteSdFile(Ios::SdCardService& sdCard, const Ios::SdPath& path, Formats::ByteView bytes);

// Makes the folder and every folder above it that is missing. CardPathBlocked when a part of the path is a file.
void MakeSdFolders(Ios::SdCardService& sdCard, const Ios::SdPath& folder);

void DeleteSdFileIfPresent(Ios::SdCardService& sdCard, const Ios::SdPath& path);

}
