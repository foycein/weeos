#include "Ios/FileSystem/NandStorage.h"

#include "Ios/FileSystem/HostNames.h"
#include "Ios/FileSystem/NandLimits.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

namespace Ios
{

namespace
{

// A file length is stored in 32 bits.
constexpr std::uintmax_t LargestFileLength = std::numeric_limits<std::uint32_t>::max();

// The root of a new NAND: owned by the super user, who may do everything. The group and everybody else may only read.
EntryRecord MakeRootRecord()
{
    EntryRecord root;
    root.kind = EntryKind::Directory;
    root.attributes = EntryAttributes{SuperUserId, SuperUserGroupId, EntryPermissions{AccessRights::None, AccessRights::Read, AccessRights::Read}, 0};
    root.linkNumber = 0;
    root.length = 0;
    return root;
}

std::optional<std::string> ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;

    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

Result<EntryMap> LoadEntries(const std::filesystem::path& nandFolder)
{
    std::filesystem::path indexPath = nandFolder / IndexFileName;
    std::error_code error;
    if (!std::filesystem::exists(indexPath, error))
    {
        EntryMap newNand;
        newNand.emplace(NandPath::Root(), MakeRootRecord());
        return newNand;
    }

    std::optional<std::string> indexText = ReadTextFile(indexPath);
    if (!indexText)
        return IosError::NoFileSystem;

    return ReadEntryIndex(*indexText);
}

std::uint64_t HighestLinkNumber(const EntryMap& entries)
{
    std::uint64_t highest = 0;
    for (const auto& [path, record] : entries)
        highest = std::max(highest, record.linkNumber);

    return highest;
}

Result<void> CreateOnHost(const std::filesystem::path& hostPath, EntryKind kind)
{
    // The PC may still hold leftovers of an entry that the index does not know. They are replaced.
    std::error_code error;
    std::filesystem::remove_all(hostPath, error);
    if (error)
        return IosError::StorageFailure;

    if (kind == EntryKind::Directory)
    {
        std::filesystem::create_directory(hostPath, error);
        if (error)
            return IosError::StorageFailure;

        return Result<void>();
    }

    std::ofstream file(hostPath, std::ios::binary);
    if (!file)
        return IosError::StorageFailure;

    return Result<void>();
}

// Moves the file or folder of an entry on the PC. A file that is there already is replaced in one step.
// A folder that is there already is removed first, because Windows cannot replace a folder.
// A new name that only differs in case from the old one is the same file or folder.
Result<void> MoveOnHost(const std::filesystem::path& oldHostPath, const std::filesystem::path& newHostPath)
{
    std::error_code error;
    bool isSameHostEntry = std::filesystem::equivalent(oldHostPath, newHostPath, error);
    bool replacesFolder = !isSameHostEntry && std::filesystem::is_directory(newHostPath, error);
    if (replacesFolder)
    {
        std::filesystem::remove_all(newHostPath, error);
        if (error)
            return IosError::StorageFailure;
    }

    std::filesystem::rename(oldHostPath, newHostPath, error);
    if (error)
        return IosError::StorageFailure;

    return Result<void>();
}

}

NandStorage::NandStorage(std::filesystem::path folder, EntryMap loadedEntries)
    : nandFolder(std::move(folder))
    , entries(std::move(loadedEntries))
    , nextLinkNumber(HighestLinkNumber(entries) + 1)
{
}

Result<NandStorage> NandStorage::Open(const std::filesystem::path& nandFolder)
{
    std::error_code error;
    std::filesystem::create_directories(nandFolder, error);
    if (error || !std::filesystem::is_directory(nandFolder))
        return IosError::NoFileSystem;

    Result<EntryMap> loadedEntries = LoadEntries(nandFolder);
    if (loadedEntries.Failed())
        return loadedEntries.Error();

    NandStorage storage(nandFolder, std::move(loadedEntries.Value()));
    storage.DropEntriesMissingOnHost();
    return storage;
}

const EntryRecord* NandStorage::Find(const NandPath& path) const
{
    auto entry = entries.find(path);
    if (entry == entries.end())
        return nullptr;

    return &entry->second;
}

std::vector<NandPath> NandStorage::ListChildren(const NandPath& directory) const
{
    // Everything inside a directory comes right after it, so the children are found on the way.
    std::vector<EntryMap::const_iterator> children;
    for (auto entry = entries.upper_bound(directory); entry != entries.end() && entry->first.IsInside(directory); ++entry)
    {
        if (entry->first.Depth() == directory.Depth() + 1)
            children.push_back(entry);
    }

    std::sort(children.begin(), children.end(), [](EntryMap::const_iterator entry, EntryMap::const_iterator otherEntry)
        { return entry->second.linkNumber > otherEntry->second.linkNumber; });

    std::vector<NandPath> childPaths;
    for (EntryMap::const_iterator child : children)
        childPaths.push_back(child->first);

    return childPaths;
}

DirectoryUsage NandStorage::MeasureUsage(const NandPath& directory) const
{
    std::uint64_t usedClusters = 0;
    std::uint32_t usedInodes = 0;
    for (auto entry = entries.lower_bound(directory); entry != entries.end() && entry->first.IsInside(directory); ++entry)
    {
        ++usedInodes;
        if (entry->second.kind == EntryKind::File)
            usedClusters += ClustersForLength(entry->second.length);
    }

    return DirectoryUsage{static_cast<std::uint32_t>(usedClusters), usedInodes};
}

Result<void> NandStorage::Create(const NandPath& path, EntryKind kind, const EntryAttributes& attributes)
{
    if (Find(path) != nullptr)
        return IosError::AlreadyExists;

    const EntryRecord* parent = Find(path.Parent());
    if (parent == nullptr)
        return IosError::NotFound;

    if (parent->kind != EntryKind::Directory)
        return IosError::InvalidArgument;

    EntryRecord record;
    record.kind = kind;
    record.attributes = attributes;
    record.linkNumber = nextLinkNumber;
    record.hostName = ChooseHostName(path.Name(), HostNamesInside(path.Parent(), path));
    record.length = 0;

    Result<void> created = CreateOnHost(HostPath(path.Parent()) / record.hostName, kind);
    if (created.Failed())
        return created;

    entries.emplace(path, record);
    ++nextLinkNumber;
    return SaveIndex();
}

Result<void> NandStorage::Remove(const NandPath& path)
{
    if (path.IsRoot())
        return IosError::InvalidArgument;

    if (Find(path) == nullptr)
        return IosError::NotFound;

    std::error_code error;
    std::filesystem::remove_all(HostPath(path), error);
    if (error)
        return IosError::StorageFailure;

    EraseSubtree(path);
    return SaveIndex();
}

Result<void> NandStorage::Move(const NandPath& from, const NandPath& to)
{
    if (from.IsRoot())
        return IosError::InvalidArgument;

    const EntryRecord* source = Find(from);
    if (source == nullptr)
        return IosError::NotFound;

    const EntryRecord* newParent = Find(to.Parent());
    if (newParent == nullptr)
        return IosError::NotFound;

    if (newParent->kind != EntryKind::Directory)
        return IosError::InvalidArgument;

    // An entry cannot move into itself. It cannot replace itself or a directory that it lies in either: that would delete the entry that moves.
    bool movesIntoItself = to.IsInside(from);
    bool replacesItsOwnDirectory = from.IsInside(to);
    if (movesIntoItself || replacesItsOwnDirectory)
        return IosError::InvalidArgument;

    const EntryRecord* replacedEntry = Find(to);
    if (replacedEntry != nullptr && replacedEntry->kind != source->kind)
        return IosError::AlreadyExists;

    // An entry that is replaced hands its host name on, so that the file on the PC can be replaced in one step.
    std::string newHostName = replacedEntry != nullptr ? replacedEntry->hostName : ChooseHostName(to.Name(), HostNamesInside(to.Parent(), from));
    Result<void> movedOnHost = MoveOnHost(HostPath(from), HostPath(to.Parent()) / newHostName);
    if (movedOnHost.Failed())
        return movedOnHost;

    if (replacedEntry != nullptr)
        EraseSubtree(to);

    std::vector<std::pair<NandPath, EntryRecord>> movedEntries = EraseSubtree(from);
    for (auto& [oldPath, record] : movedEntries)
    {
        if (oldPath == from)
        {
            record.hostName = newHostName;
            record.linkNumber = nextLinkNumber;
            ++nextLinkNumber;
        }

        entries.emplace(oldPath.AfterMove(from, to), record);
    }

    return SaveIndex();
}

Result<void> NandStorage::SetAttributes(const NandPath& path, const EntryAttributes& attributes)
{
    auto entry = entries.find(path);
    if (entry == entries.end())
        return IosError::NotFound;

    entry->second.attributes = attributes;
    return SaveIndex();
}

void NandStorage::SetLength(const NandPath& path, std::uint32_t length)
{
    entries.at(path).length = length;
}

std::filesystem::path NandStorage::HostPath(const NandPath& path) const
{
    std::vector<std::string> hostNames;
    for (NandPath current = path; !current.IsRoot(); current = current.Parent())
        hostNames.push_back(entries.at(current).hostName);

    std::filesystem::path hostPath = nandFolder;
    for (auto hostName = hostNames.rbegin(); hostName != hostNames.rend(); ++hostName)
        hostPath /= *hostName;

    return hostPath;
}

// The index says which entries exist, but a person may have deleted files on the PC. Such an entry is gone.
// The entries are visited parents first, so the host path of an entry can be built when its turn comes.
void NandStorage::DropEntriesMissingOnHost()
{
    auto entry = entries.begin();
    while (entry != entries.end())
    {
        bool keep = entry->first.IsRoot() || IsPresentOnHost(entry->first, entry->second);
        if (keep)
            ++entry;
        else
            entry = entries.erase(entry);
    }
}

// True when the PC has the file or folder of the entry. A file gets its length from the PC file.
bool NandStorage::IsPresentOnHost(const NandPath& path, EntryRecord& record) const
{
    if (!entries.contains(path.Parent()))
        return false;

    std::filesystem::path hostPath = HostPath(path);
    std::error_code error;
    if (record.kind == EntryKind::Directory)
        return std::filesystem::is_directory(hostPath, error);

    if (!std::filesystem::is_regular_file(hostPath, error))
        return false;

    std::uintmax_t fileSize = std::filesystem::file_size(hostPath, error);
    if (error || fileSize > LargestFileLength)
        return false;

    record.length = static_cast<std::uint32_t>(fileSize);
    return true;
}

// Takes an entry and everything inside it out of the entries and gives them back. They lie together, the entry first.
std::vector<std::pair<NandPath, EntryRecord>> NandStorage::EraseSubtree(const NandPath& path)
{
    std::vector<std::pair<NandPath, EntryRecord>> erasedEntries;
    auto first = entries.find(path);
    auto end = first;
    while (end != entries.end() && end->first.IsInside(path))
    {
        erasedEntries.emplace_back(end->first, end->second);
        ++end;
    }

    entries.erase(first, end);
    return erasedEntries;
}

// The host names of the entries in a directory, not counting one entry. That is the entry that is about to be placed there.
std::vector<std::string> NandStorage::HostNamesInside(const NandPath& directory, const NandPath& ignoredEntry) const
{
    std::vector<std::string> hostNames;
    for (const NandPath& child : ListChildren(directory))
    {
        if (child != ignoredEntry)
            hostNames.push_back(entries.at(child).hostName);
    }
    return hostNames;
}

Result<void> NandStorage::SaveIndex() const
{
    std::filesystem::path indexPath = nandFolder / IndexFileName;
    std::filesystem::path temporaryPath = nandFolder / IndexTemporaryFileName;

    std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
    file << WriteEntryIndex(entries);
    file.close();
    if (!file)
        return IosError::StorageFailure;

    std::error_code error;
    std::filesystem::rename(temporaryPath, indexPath, error);
    if (error)
        return IosError::StorageFailure;

    return Result<void>();
}

}
