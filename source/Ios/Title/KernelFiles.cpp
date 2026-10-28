#include "Ios/Title/KernelFiles.h"

namespace Ios
{

namespace
{

constexpr std::uint8_t NoFlags = 0;

}

KernelFiles::KernelFiles(FileSystemService& fileSystem)
    : fileSystem(fileSystem)
{
}

bool KernelFiles::Exists(const NandPath& path) const
{
    return fileSystem.GetAttributes(path, SuperUserCaller).Succeeded();
}

Result<Formats::Bytes> KernelFiles::ReadFile(const NandPath& path) const
{
    Result<OpenFile> opened = fileSystem.Open(path, OpenMode::Read, SuperUserCaller);
    if (opened.Failed())
        return opened.Error();

    Result<FileStatistics> statistics = opened.Value().GetStatistics();
    if (statistics.Failed())
        return statistics.Error();

    return opened.Value().Read(statistics.Value().length);
}

Result<void> KernelFiles::WriteFile(const NandPath& path, Formats::ByteView bytes)
{
    Result<void> removed = DeleteIfPresent(path);
    if (removed.Failed())
        return removed;

    Result<void> created = fileSystem.CreateFile(path, SystemEntryPermissions, NoFlags, SuperUserCaller);
    if (created.Failed())
        return created;

    Result<OpenFile> opened = fileSystem.Open(path, OpenMode::Write, SuperUserCaller);
    if (opened.Failed())
    {
        fileSystem.DeleteEntry(path, SuperUserCaller);
        return opened.Error();
    }

    Result<std::size_t> written = opened.Value().Write(bytes);
    opened.Value().Close();
    if (written.Failed())
    {
        fileSystem.DeleteEntry(path, SuperUserCaller);
        return written.Error();
    }

    return Result<void>();
}

Result<std::vector<std::string>> KernelFiles::ListNames(const NandPath& directory) const
{
    return fileSystem.ListDirectory(directory, SuperUserCaller);
}

Result<void> KernelFiles::EnsureDirectory(const NandPath& path, const EntryPermissions& permissions)
{
    Result<void> created = fileSystem.CreateDirectory(path, permissions, NoFlags, SuperUserCaller);
    if (created.Failed() && created.Error() != IosError::AlreadyExists)
        return created;

    return Result<void>();
}

Result<void> KernelFiles::Delete(const NandPath& path)
{
    return fileSystem.DeleteEntry(path, SuperUserCaller);
}

Result<void> KernelFiles::DeleteIfPresent(const NandPath& path)
{
    Result<void> deleted = fileSystem.DeleteEntry(path, SuperUserCaller);
    if (deleted.Failed() && deleted.Error() != IosError::NotFound)
        return deleted;

    return Result<void>();
}

Result<void> KernelFiles::Rename(const NandPath& from, const NandPath& to)
{
    return fileSystem.RenameEntry(from, to, SuperUserCaller);
}

Result<void> KernelFiles::SetOwner(const NandPath& path, std::uint32_t userId, std::uint16_t groupId)
{
    Result<EntryAttributes> current = fileSystem.GetAttributes(path, SuperUserCaller);
    if (current.Failed())
        return current.Error();

    EntryAttributes changed = current.Value();
    changed.ownerId = userId;
    changed.groupId = groupId;
    return fileSystem.SetAttributes(path, changed, SuperUserCaller);
}

}
