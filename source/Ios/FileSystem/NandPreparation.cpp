#include "Ios/FileSystem/NandPreparation.h"

#include <string_view>

namespace Ios
{

namespace
{

constexpr AccessRights ReadWrite = AccessRights::ReadWrite;
constexpr AccessRights Read = AccessRights::Read;
constexpr AccessRights None = AccessRights::None;

constexpr EntryPermissions SystemOnly = {ReadWrite, ReadWrite, None};
constexpr EntryPermissions ReadableByEveryone = {ReadWrite, ReadWrite, Read};
constexpr EntryPermissions OpenToEveryone = {ReadWrite, ReadWrite, ReadWrite};
constexpr EntryPermissions OwnerOnly = {ReadWrite, None, None};

constexpr std::uint8_t NoFlags = 0;

constexpr std::string_view TemporaryDirectoryPath = "/tmp";
constexpr std::string_view SystemMenuDirectoryPath = "/meta";
constexpr std::string_view WfsDirectoryPath = "/wfs";

struct StandardDirectory
{
    std::string_view path;
    EntryPermissions permissions;
};

// The eight directories that every title service makes sure of, in its order, with the permissions it gives them.
constexpr StandardDirectory StandardDirectories[] =
{
    {"/sys", SystemOnly},
    {"/ticket", SystemOnly},
    {"/title", ReadableByEveryone},
    {"/shared1", SystemOnly},
    {"/shared2", OpenToEveryone},
    {TemporaryDirectoryPath, OpenToEveryone},
    {"/import", SystemOnly},
    {SystemMenuDirectoryPath, OpenToEveryone},
};

// The ninth, which the title service of 2009 and later makes.
constexpr StandardDirectory WfsDirectory = {WfsDirectoryPath, OwnerOnly};

NandPath ParseConstantPath(std::string_view text)
{
    return NandPath::Parse(text).Value();
}

EntryAttributes SuperUserAttributes(const EntryPermissions& permissions)
{
    return EntryAttributes{SuperUserId, SuperUserGroupId, permissions, NoFlags};
}

Result<void> CreateDirectoryForSuperUser(NandStorage& storage, const NandPath& path, const EntryPermissions& permissions)
{
    return storage.Create(path, EntryKind::Directory, SuperUserAttributes(permissions));
}

// The file system module deletes /tmp and makes it again when it holds anything, and makes it when it is missing.
// The module of IOS9 does not look: it deletes /tmp and makes it again every time.
Result<void> EmptyTemporaryDirectory(NandStorage& storage, bool recreatesAtEveryStart)
{
    NandPath path = ParseConstantPath(TemporaryDirectoryPath);
    bool exists = storage.Find(path) != nullptr;
    bool isEmpty = exists && storage.ListChildren(path).empty();
    if (isEmpty && !recreatesAtEveryStart)
        return Result<void>();

    if (exists)
    {
        Result<void> removed = storage.Remove(path);
        if (removed.Failed())
            return removed;
    }

    return CreateDirectoryForSuperUser(storage, path, OpenToEveryone);
}

Result<void> MakeSureDirectoryExists(NandStorage& storage, const StandardDirectory& directory)
{
    NandPath path = ParseConstantPath(directory.path);
    if (storage.Find(path) != nullptr)
        return Result<void>();

    return CreateDirectoryForSuperUser(storage, path, directory.permissions);
}

Result<void> MakeSureStandardDirectoriesExist(NandStorage& storage, bool makesWfsDirectory)
{
    for (const StandardDirectory& directory : StandardDirectories)
    {
        Result<void> made = MakeSureDirectoryExists(storage, directory);
        if (made.Failed())
            return made;
    }

    if (!makesWfsDirectory)
        return Result<void>();

    return MakeSureDirectoryExists(storage, WfsDirectory);
}

Result<void> GiveDirectoryToOwner(NandStorage& storage, std::string_view path, const EntryAttributes& attributes)
{
    return storage.SetAttributes(ParseConstantPath(path), attributes);
}

}

Result<void> PrepareNandFolder(NandStorage& storage, const IosProfile& profile)
{
    Result<void> emptied = EmptyTemporaryDirectory(storage, profile.fileSystemRecreatesTmpAtEveryStart);
    if (emptied.Failed())
        return emptied;

    Result<void> made = MakeSureStandardDirectoriesExist(storage, profile.esCreatesWfsFolder);
    if (made.Failed())
        return made;

    Result<void> systemMenuGiven = GiveDirectoryToOwner(storage, SystemMenuDirectoryPath,
        EntryAttributes{SystemMenuUserId, SystemMenuGroupId, OpenToEveryone, NoFlags});
    if (systemMenuGiven.Failed())
        return systemMenuGiven;

    if (!profile.esCreatesWfsFolder)
        return Result<void>();

    return GiveDirectoryToOwner(storage, WfsDirectoryPath, EntryAttributes{WfsUserId, WfsGroupId, OwnerOnly, NoFlags});
}

}
