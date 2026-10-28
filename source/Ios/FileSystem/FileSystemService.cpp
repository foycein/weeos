#include "Ios/FileSystem/FileSystemService.h"

#include "Ios/FileSystem/FileSystemState.h"

#include <ios>
#include <mutex>
#include <utility>

namespace Ios
{

// Each public call locks the state and hands over to one function below. The functions check things in the order
// that the file system module of IOS80 does, because the first problem decides which error the caller sees.
namespace
{

Result<const EntryRecord*> FindEntry(const FileSystemState& state, const NandPath& path)
{
    const EntryRecord* entry = state.storage.Find(path);
    if (entry == nullptr)
        return IosError::NotFound;

    return entry;
}

Result<void> RequireRights(const EntryRecord& entry, const Caller& caller, AccessRights required)
{
    if (!HasRights(entry.attributes, caller, required))
        return IosError::AccessDenied;

    return Result<void>();
}

bool IsNameTooLong(const NandPath& path)
{
    return path.Name().size() > MaximumNameLength;
}

// The directory that holds the entry, when the caller has these rights on it.
Result<const EntryRecord*> FindParentWithRights(const FileSystemState& state, const NandPath& path, const Caller& caller, AccessRights required)
{
    Result<const EntryRecord*> parent = FindEntry(state, path.Parent());
    if (parent.Failed())
        return parent;

    Result<void> allowed = RequireRights(*parent.Value(), caller, required);
    if (allowed.Failed())
        return allowed.Error();

    return parent;
}

Result<void> CreateNewEntry(FileSystemState& state, const NandPath& path, EntryKind kind, const EntryPermissions& permissions, std::uint8_t flags, const Caller& caller)
{
    if (state.isShutDown)
        return IosError::ShutDown;

    if (path.IsRoot())
        return IosError::InvalidArgument;

    bool isTooDeep = kind == EntryKind::Directory && path.Depth() > MaximumDirectoryDepth;
    if (isTooDeep)
        return IosError::PathTooDeep;

    if (IsNameTooLong(path))
        return IosError::InvalidArgument;

    Result<const EntryRecord*> parent = FindParentWithRights(state, path, caller, AccessRights::Write);
    if (parent.Failed())
        return parent.Error();

    if (parent.Value()->kind != EntryKind::Directory)
        return IosError::InvalidArgument;

    if (state.storage.Find(path) != nullptr)
        return IosError::AlreadyExists;

    if (state.FreeInodeCount() == 0)
        return IosError::NoFreeInodes;

    EntryAttributes attributes = {caller.userId, caller.groupId, permissions, flags};
    return state.storage.Create(path, kind, attributes);
}

Result<void> RemoveEntry(FileSystemState& state, const NandPath& path, const Caller& caller)
{
    if (state.isShutDown)
        return IosError::ShutDown;

    if (path.IsRoot() || IsNameTooLong(path))
        return IosError::InvalidArgument;

    Result<const EntryRecord*> parent = FindParentWithRights(state, path, caller, AccessRights::Write);
    if (parent.Failed())
        return parent.Error();

    Result<const EntryRecord*> entry = FindEntry(state, path);
    if (entry.Failed())
        return entry.Error();

    if (state.IsAnyOpenInside(path))
        return IosError::FileInUse;

    return state.storage.Remove(path);
}

// What must be true about the entry that is replaced when an entry moves onto it.
Result<void> CheckReplacedEntry(const FileSystemState& state, const NandPath& from, const EntryRecord& source, const NandPath& to)
{
    const EntryRecord* replacedEntry = state.storage.Find(to);
    if (replacedEntry == nullptr)
        return Result<void>();

    if (replacedEntry->kind != source.kind || to == from)
        return IosError::InvalidArgument;

    if (state.IsAnyOpenInside(to))
        return IosError::FileInUse;

    return Result<void>();
}

// Both directories must exist, and the caller must have the right to write in both.
Result<void> RequireWritableDirectories(const FileSystemState& state, const NandPath& from, const NandPath& to, const Caller& caller)
{
    Result<const EntryRecord*> oldParent = FindEntry(state, from.Parent());
    if (oldParent.Failed())
        return oldParent.Error();

    Result<const EntryRecord*> newParent = FindEntry(state, to.Parent());
    if (newParent.Failed())
        return newParent.Error();

    Result<void> mayChangeOldParent = RequireRights(*oldParent.Value(), caller, AccessRights::Write);
    if (mayChangeOldParent.Failed())
        return mayChangeOldParent;

    return RequireRights(*newParent.Value(), caller, AccessRights::Write);
}

Result<void> MoveEntry(FileSystemState& state, const NandPath& from, const NandPath& to, const Caller& caller)
{
    if (state.isShutDown)
        return IosError::ShutDown;

    if (from.IsRoot() || to.IsRoot())
        return IosError::InvalidArgument;

    if (IsNameTooLong(from) || IsNameTooLong(to))
        return IosError::InvalidArgument;

    Result<void> directoriesAllowIt = RequireWritableDirectories(state, from, to, caller);
    if (directoriesAllowIt.Failed())
        return directoriesAllowIt;

    Result<const EntryRecord*> source = FindEntry(state, from);
    if (source.Failed())
        return source.Error();

    // The console renames directories only. A file can move to another directory, but it keeps its name.
    bool renamesFile = source.Value()->kind == EntryKind::File && from.Name() != to.Name();
    if (renamesFile)
        return IosError::InvalidArgument;

    if (state.IsAnyOpenInside(from))
        return IosError::FileInUse;

    if (state.storage.Find(to.Parent())->kind != EntryKind::Directory)
        return IosError::InvalidArgument;

    Result<void> replacedEntryAllowsIt = CheckReplacedEntry(state, from, *source.Value(), to);
    if (replacedEntryAllowsIt.Failed())
        return replacedEntryAllowsIt;

    return state.storage.Move(from, to);
}

Result<void> ChangeAttributes(FileSystemState& state, const NandPath& path, const EntryAttributes& attributes, const Caller& caller)
{
    if (state.isShutDown)
        return IosError::ShutDown;

    Result<const EntryRecord*> entry = FindEntry(state, path);
    if (entry.Failed())
        return entry.Error();

    const EntryRecord& record = *entry.Value();
    bool isSuperUser = caller.userId == SuperUserId;
    bool isOwner = caller.userId == record.attributes.ownerId;
    if (!isSuperUser && !isOwner)
        return IosError::AccessDenied;

    bool changesOwner = attributes.ownerId != record.attributes.ownerId;
    if (changesOwner && !isSuperUser)
        return IosError::AccessDenied;

    bool changesOwnerOfFileWithData = changesOwner && record.kind == EntryKind::File && record.length != 0;
    if (changesOwnerOfFileWithData)
        return IosError::NotEmpty;

    return state.storage.SetAttributes(path, attributes);
}

Result<EntryAttributes> ReadAttributes(const FileSystemState& state, const NandPath& path, const Caller& caller)
{
    if (state.isShutDown)
        return IosError::ShutDown;

    // Nobody is asked about the root.
    if (path.IsRoot())
        return state.storage.Find(path)->attributes;

    if (IsNameTooLong(path))
        return IosError::InvalidArgument;

    Result<const EntryRecord*> parent = FindParentWithRights(state, path, caller, AccessRights::Read);
    if (parent.Failed())
        return parent.Error();

    Result<const EntryRecord*> entry = FindEntry(state, path);
    if (entry.Failed())
        return entry.Error();

    return entry.Value()->attributes;
}

Result<std::vector<std::string>> ReadDirectoryNames(const FileSystemState& state, const NandPath& directory, const Caller& caller)
{
    if (state.isShutDown)
        return IosError::ShutDown;

    Result<const EntryRecord*> entry = FindEntry(state, directory);
    if (entry.Failed())
        return entry.Error();

    if (entry.Value()->kind != EntryKind::Directory)
        return IosError::InvalidArgument;

    Result<void> mayRead = RequireRights(*entry.Value(), caller, AccessRights::Read);
    if (mayRead.Failed())
        return mayRead.Error();

    std::vector<std::string> names;
    for (const NandPath& child : state.storage.ListChildren(directory))
        names.emplace_back(child.Name());

    return names;
}

Result<DirectoryUsage> MeasureDirectory(const FileSystemState& state, const NandPath& directory)
{
    if (state.isShutDown)
        return IosError::ShutDown;

    Result<const EntryRecord*> entry = FindEntry(state, directory);
    if (entry.Failed())
        return entry.Error();

    if (entry.Value()->kind != EntryKind::Directory)
        return IosError::InvalidArgument;

    return state.storage.MeasureUsage(directory);
}

// The file is opened on the PC for reading, or for reading and writing. A file that is opened only to write must not be emptied.
std::ios::openmode HostOpenMode(OpenMode mode)
{
    if (mode == OpenMode::Read)
        return std::ios::in | std::ios::binary;

    return std::ios::in | std::ios::out | std::ios::binary;
}

}

FileSystemService::FileSystemService(NandStorage storage, const VolumeCapacity& capacity)
    : state(std::make_shared<FileSystemState>(std::move(storage), capacity))
{
}

Result<VolumeStatistics> FileSystemService::GetVolumeStatistics() const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->isShutDown)
        return IosError::ShutDown;

    DirectoryUsage usage = state->storage.MeasureUsage(NandPath::Root());

    VolumeStatistics statistics;
    statistics.clusterSize = ClusterSize;
    statistics.freeClusters = state->FreeClusterCount();
    statistics.usedClusters = usage.usedClusters;
    statistics.badClusters = state->capacity.badClusterCount;
    statistics.reservedClusters = state->capacity.reservedClusterCount;
    statistics.freeInodes = state->FreeInodeCount();
    statistics.usedInodes = usage.usedInodes;
    return statistics;
}

Result<DirectoryUsage> FileSystemService::GetDirectoryUsage(const NandPath& directory) const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return MeasureDirectory(*state, directory);
}

Result<void> FileSystemService::CreateDirectory(const NandPath& path, const EntryPermissions& permissions, std::uint8_t flags, const Caller& caller)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return CreateNewEntry(*state, path, EntryKind::Directory, permissions, flags, caller);
}

Result<void> FileSystemService::CreateFile(const NandPath& path, const EntryPermissions& permissions, std::uint8_t flags, const Caller& caller)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return CreateNewEntry(*state, path, EntryKind::File, permissions, flags, caller);
}

Result<std::vector<std::string>> FileSystemService::ListDirectory(const NandPath& directory, const Caller& caller) const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return ReadDirectoryNames(*state, directory, caller);
}

Result<EntryAttributes> FileSystemService::GetAttributes(const NandPath& path, const Caller& caller) const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return ReadAttributes(*state, path, caller);
}

Result<void> FileSystemService::SetAttributes(const NandPath& path, const EntryAttributes& attributes, const Caller& caller)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return ChangeAttributes(*state, path, attributes, caller);
}

Result<void> FileSystemService::DeleteEntry(const NandPath& path, const Caller& caller)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return RemoveEntry(*state, path, caller);
}

Result<void> FileSystemService::RenameEntry(const NandPath& from, const NandPath& to, const Caller& caller)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return MoveEntry(*state, from, to, caller);
}

Result<OpenFile> FileSystemService::Open(const NandPath& path, OpenMode mode, const Caller& caller)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->isShutDown)
        return IosError::ShutDown;

    if (path.IsRoot())
        return IosError::InvalidArgument;

    Result<const EntryRecord*> entry = FindEntry(*state, path);
    if (entry.Failed())
        return entry.Error();

    if (entry.Value()->kind != EntryKind::File)
        return IosError::InvalidArgument;

    Result<void> mayOpen = RequireRights(*entry.Value(), caller, RightsNeededToOpen(mode));
    if (mayOpen.Failed())
        return mayOpen.Error();

    if (state->openFilePaths.size() >= MaximumOpenFiles)
        return IosError::TooManyOpenFiles;

    std::fstream hostStream(state->storage.HostPath(path), HostOpenMode(mode));
    if (!hostStream.is_open())
        return IosError::StorageFailure;

    state->openFilePaths.push_back(path);
    return OpenFile(state, path, mode, std::move(hostStream));
}

void FileSystemService::Shutdown()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    state->isShutDown = true;
}

}
