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

// The nine directories the title service makes sure of, in its order, with the permissions it gives them.
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
    {WfsDirectoryPath, OwnerOnly},
};

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
Result<void> EmptyTemporaryDirectory(NandStorage& storage)
{
    NandPath path = ParseConstantPath(TemporaryDirectoryPath);
    bool exists = storage.Find(path) != nullptr;
    if (exists && storage.ListChildren(path).empty())
        return Result<void>();

    if (exists)
    {
        Result<void> removed = storage.Remove(path);
        if (removed.Failed())
            return removed;
    }

    return CreateDirectoryForSuperUser(storage, path, OpenToEveryone);
}

Result<void> MakeSureStandardDirectoriesExist(NandStorage& storage)
{
    for (const StandardDirectory& directory : StandardDirectories)
    {
        NandPath path = ParseConstantPath(directory.path);
        if (storage.Find(path) != nullptr)
            continue;

        Result<void> created = CreateDirectoryForSuperUser(storage, path, directory.permissions);
        if (created.Failed())
            return created;
    }

    return Result<void>();
}

Result<void> GiveDirectoryToOwner(NandStorage& storage, std::string_view path, const EntryAttributes& attributes)
{
    return storage.SetAttributes(ParseConstantPath(path), attributes);
}

}

Result<void> PrepareNandFolder(NandStorage& storage)
{
    Result<void> emptied = EmptyTemporaryDirectory(storage);
    if (emptied.Failed())
        return emptied;

    Result<void> made = MakeSureStandardDirectoriesExist(storage);
    if (made.Failed())
        return made;

    Result<void> systemMenuGiven = GiveDirectoryToOwner(storage, SystemMenuDirectoryPath,
        EntryAttributes{SystemMenuUserId, SystemMenuGroupId, OpenToEveryone, NoFlags});
    if (systemMenuGiven.Failed())
        return systemMenuGiven;

    return GiveDirectoryToOwner(storage, WfsDirectoryPath, EntryAttributes{WfsUserId, WfsGroupId, OwnerOnly, NoFlags});
}

}
