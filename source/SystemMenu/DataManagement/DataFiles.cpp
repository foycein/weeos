#include "SystemMenu/DataManagement/DataFiles.h"

#include <format>

namespace SystemMenu
{

namespace
{

constexpr int OwnerShift = 4;
constexpr int GroupShift = 2;
constexpr int OtherShift = 0;
constexpr std::uint8_t RightsMask = 0x3;

std::uint8_t RightsBits(Ios::AccessRights rights)
{
    return static_cast<std::uint8_t>(rights);
}

Ios::AccessRights RightsAt(std::uint8_t permissionsByte, int shift)
{
    return static_cast<Ios::AccessRights>((permissionsByte >> shift) & RightsMask);
}

}

DataResult ResultOfIosError(Ios::IosError error)
{
    switch (error)
    {
    case Ios::IosError::NoFreeClusters:
    case Ios::IosError::NoFreeInodes:
        return DataResult::NotEnoughRoom;
    default:
        return DataResult::Failed;
    }
}

DataResult ResultOfCardError(Ios::IosError error)
{
    if (error == Ios::IosError::AccessDenied)
        return DataResult::CardLocked;

    return ResultOfIosError(error);
}

void Require(Ios::Result<void> result, std::string_view whatWasDone)
{
    if (result.Failed())
        throw DataError(ResultOfIosError(result.Error()), std::string(whatWasDone));
}

void RequireOnCard(Ios::Result<void> result, std::string_view whatWasDone)
{
    if (result.Failed())
        throw DataError(ResultOfCardError(result.Error()), std::string(whatWasDone));
}

Ios::NandPath MakeNandPath(std::string_view text)
{
    return Require(Ios::NandPath::Parse(text), std::format("make the NAND path {}", text));
}

Ios::NandPath ChildOf(const Ios::NandPath& directory, std::string_view name)
{
    return Require(directory.Child(name), std::format("name {} in {}", name, directory.ToString()));
}

Ios::NandPath MetaFolderPath(Ios::TitleId titleId)
{
    return MakeNandPath(std::format("/meta/{:08x}/{:08x}", titleId.highPart, titleId.lowPart));
}

NandAccess::NandAccess(Ios::FileSystemService& nandFileSystem, const Ios::Caller& nandCaller)
    : fileSystem(nandFileSystem)
    , caller(nandCaller)
{
}

bool NandAccess::Exists(const Ios::NandPath& path) const
{
    return fileSystem.GetAttributes(path, caller).Succeeded();
}

// Only a directory has a usage.
Ios::EntryKind NandAccess::KindOf(const Ios::NandPath& path) const
{
    if (fileSystem.GetDirectoryUsage(path).Succeeded())
        return Ios::EntryKind::Directory;

    return Ios::EntryKind::File;
}

Ios::EntryAttributes NandAccess::AttributesOf(const Ios::NandPath& path) const
{
    return Require(fileSystem.GetAttributes(path, caller), std::format("read the attributes of {}", path.ToString()));
}

std::vector<std::string> NandAccess::List(const Ios::NandPath& directory) const
{
    return Require(fileSystem.ListDirectory(directory, caller), std::format("list {}", directory.ToString()));
}

Formats::Bytes NandAccess::ReadFile(const Ios::NandPath& path) const
{
    std::string what = std::format("read {}", path.ToString());
    Ios::OpenFile file = Require(fileSystem.Open(path, Ios::OpenMode::Read, caller), what);
    Ios::FileStatistics statistics = Require(file.GetStatistics(), what);
    return Require(file.Read(statistics.length), what);
}

void NandAccess::WriteNewFile(const Ios::NandPath& path, Formats::ByteView bytes, const Ios::EntryPermissions& permissions, std::uint8_t flags)
{
    std::string what = std::format("write {}", path.ToString());
    Require(fileSystem.CreateFile(path, permissions, flags, caller), what);
    Ios::OpenFile file = Require(fileSystem.Open(path, Ios::OpenMode::Write, caller), what);
    Require(file.Write(bytes), what);
    Require(file.Close(), what);
}

void NandAccess::CreateDirectory(const Ios::NandPath& path, const Ios::EntryPermissions& permissions, std::uint8_t flags)
{
    Require(fileSystem.CreateDirectory(path, permissions, flags, caller), std::format("make {}", path.ToString()));
}

void NandAccess::SetPermissions(const Ios::NandPath& path, const Ios::EntryPermissions& permissions)
{
    Ios::EntryAttributes attributes = AttributesOf(path);
    attributes.permissions = permissions;
    Require(fileSystem.SetAttributes(path, attributes, caller), std::format("set the rights of {}", path.ToString()));
}

void NandAccess::DeleteIfPresent(const Ios::NandPath& path)
{
    if (!Exists(path))
        return;

    Require(fileSystem.DeleteEntry(path, caller), std::format("delete {}", path.ToString()));
}

void NandAccess::Move(const Ios::NandPath& from, const Ios::NandPath& to)
{
    Require(fileSystem.RenameEntry(from, to, caller), std::format("move {} to {}", from.ToString(), to.ToString()));
}

std::uint8_t PermissionsToByte(const Ios::EntryPermissions& permissions)
{
    std::uint8_t ownerBits = static_cast<std::uint8_t>(RightsBits(permissions.owner) << OwnerShift);
    std::uint8_t groupBits = static_cast<std::uint8_t>(RightsBits(permissions.group) << GroupShift);
    std::uint8_t otherBits = static_cast<std::uint8_t>(RightsBits(permissions.other) << OtherShift);
    return ownerBits | groupBits | otherBits;
}

Ios::EntryPermissions PermissionsFromByte(std::uint8_t permissionsByte)
{
    Ios::EntryPermissions permissions;
    permissions.owner = RightsAt(permissionsByte, OwnerShift);
    permissions.group = RightsAt(permissionsByte, GroupShift);
    permissions.other = RightsAt(permissionsByte, OtherShift);
    return permissions;
}

Ios::SdPath MakeSdPath(std::string_view text)
{
    return RequireOnCard(Ios::SdPath::Parse(text), std::format("make the SD card path {}", text));
}

std::optional<std::uint64_t> SdFileSize(const Ios::SdCardService& sdCard, const Ios::SdPath& path)
{
    Ios::Result<Ios::SdEntry> entry = sdCard.GetEntryStatus(path);
    if (entry.Failed() || entry.Value().kind != Ios::EntryKind::File)
        return std::nullopt;

    return entry.Value().length;
}

Formats::Bytes ReadSdFile(Ios::SdCardService& sdCard, const Ios::SdPath& path)
{
    std::string what = std::format("read {} from the SD card", path.ToString());
    Ios::SdFile file = RequireOnCard(sdCard.Open(path, Ios::OpenMode::Read), what);
    std::uint32_t length = RequireOnCard(file.GetLength(), what);
    return RequireOnCard(file.Read(length), what);
}

Formats::Bytes ReadSdFileStart(Ios::SdCardService& sdCard, const Ios::SdPath& path, std::size_t count)
{
    std::string what = std::format("read {} from the SD card", path.ToString());
    Ios::SdFile file = RequireOnCard(sdCard.Open(path, Ios::OpenMode::Read), what);
    return RequireOnCard(file.Read(count), what);
}

void WriteSdFile(Ios::SdCardService& sdCard, const Ios::SdPath& path, Formats::ByteView bytes)
{
    std::string what = std::format("write {} to the SD card", path.ToString());
    DeleteSdFileIfPresent(sdCard, path);
    RequireOnCard(sdCard.CreateFile(path), what);
    Ios::SdFile file = RequireOnCard(sdCard.Open(path, Ios::OpenMode::Write), what);
    RequireOnCard(file.Write(bytes), what);
    RequireOnCard(file.Close(), what);
}

void MakeSdFolders(Ios::SdCardService& sdCard, const Ios::SdPath& folder)
{
    if (folder.IsRoot())
        return;

    Ios::Result<Ios::SdEntry> entry = sdCard.GetEntryStatus(folder);
    if (entry.Succeeded())
    {
        if (entry.Value().kind != Ios::EntryKind::Directory)
            throw DataError(DataResult::CardPathBlocked, std::format("{} on the SD card is a file", folder.ToString()));
        return;
    }

    MakeSdFolders(sdCard, folder.Parent());
    RequireOnCard(sdCard.CreateDirectory(folder), std::format("make {} on the SD card", folder.ToString()));
}

void DeleteSdFileIfPresent(Ios::SdCardService& sdCard, const Ios::SdPath& path)
{
    if (!sdCard.GetEntryStatus(path).Succeeded())
        return;

    RequireOnCard(sdCard.DeleteEntry(path), std::format("delete {} from the SD card", path.ToString()));
}

}
