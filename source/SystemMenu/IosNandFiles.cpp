#include "SystemMenu/IosNandFiles.h"

#include "SystemMenu/MenuError.h"

#include <format>
#include <string>

namespace SystemMenu
{

namespace
{

// The menu makes its files like this (state.dat: 0x815376B4).
constexpr Ios::EntryPermissions EverybodyReadsAndWrites{Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite};
constexpr std::uint8_t NoAttributeFlags = 0;

// The safe save of the NAND library (0x815655F8, 0x81565BA8) writes into /tmp/sys/<number>/<name>, then moves the file into place.
constexpr std::string_view SafeSaveFolderPath = "/tmp/sys";
constexpr Ios::EntryPermissions OwnerOnly{Ios::AccessRights::ReadWrite, Ios::AccessRights::None, Ios::AccessRights::None};

MenuError MakeNandError(std::string_view action, std::string_view path, Ios::IosError error)
{
    std::string message = std::format("The menu cannot {} the file {} (result code {})", action, path, Ios::OriginalResultCode(error));
    return MenuError(message, FatalErrorOf(error));
}

}

IosNandFiles::IosNandFiles(Ios::FileSystemService& fileSystemService, const Ios::TitleService& titleService)
    : fileSystem(fileSystemService)
    , titles(&titleService)
{
}

IosNandFiles::IosNandFiles(Ios::FileSystemService& fileSystemService, Ios::Caller fixedCaller)
    : fileSystem(fileSystemService)
    , caller(fixedCaller)
{
}

std::optional<Formats::Bytes> IosNandFiles::ReadFile(std::string_view path)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(ParsePath(path), Ios::OpenMode::Read, CurrentCaller());
    if (opened.Failed() && opened.Error() == Ios::IosError::NotFound)
        return std::nullopt;

    if (opened.Failed())
        throw MakeNandError("open", path, opened.Error());

    Ios::Result<Ios::FileStatistics> statistics = opened.Value().GetStatistics();
    if (statistics.Failed())
        throw MakeNandError("measure", path, statistics.Error());

    Ios::Result<Formats::Bytes> content = opened.Value().Read(statistics.Value().length);
    if (content.Failed())
        throw MakeNandError("read", path, content.Error());

    return content.Value();
}

void IosNandFiles::DeleteFile(std::string_view path)
{
    Ios::Result<void> deleted = fileSystem.DeleteEntry(ParsePath(path), CurrentCaller());
    if (deleted.Failed() && deleted.Error() != Ios::IosError::NotFound)
        throw MakeNandError("delete", path, deleted.Error());
}

// The old file stays whole until the new one is complete: the power may go off while it is written. What is left in /tmp
// then is removed by IOS at its next start.
void IosNandFiles::WriteFile(std::string_view path, Formats::ByteView bytes)
{
    Ios::NandPath finalPath = ParsePath(path);
    Ios::NandPath waitingFolder = MakeSafeSaveFolder(path);
    Ios::NandPath waitingPath = waitingFolder.Child(finalPath.Name()).Value();

    WriteNewFile(waitingPath, path, bytes);

    Ios::Result<void> moved = fileSystem.RenameEntry(waitingPath, finalPath, CurrentCaller());
    if (moved.Failed())
        throw MakeNandError("move into place", path, moved.Error());

    fileSystem.DeleteEntry(waitingFolder, CurrentCaller());
}

Ios::NandPath IosNandFiles::MakeSafeSaveFolder(std::string_view pathForErrors)
{
    Ios::NandPath safeSaveFolder = ParsePath(SafeSaveFolderPath);
    Ios::Result<void> safeSaveFolderMade = fileSystem.CreateDirectory(safeSaveFolder, EverybodyReadsAndWrites, NoAttributeFlags, CurrentCaller());
    if (safeSaveFolderMade.Failed() && safeSaveFolderMade.Error() != Ios::IosError::AlreadyExists)
        throw MakeNandError("prepare the safe save of", pathForErrors, safeSaveFolderMade.Error());

    std::uint32_t folderNumber = nextSafeSaveNumber++;
    Ios::NandPath waitingFolder = safeSaveFolder.Child(std::format("{:08x}", folderNumber)).Value();
    Ios::Result<void> folderMade = fileSystem.CreateDirectory(waitingFolder, OwnerOnly, NoAttributeFlags, CurrentCaller());
    if (folderMade.Failed())
        throw MakeNandError("prepare the safe save of", pathForErrors, folderMade.Error());

    return waitingFolder;
}

void IosNandFiles::WriteNewFile(const Ios::NandPath& nandPath, std::string_view pathForErrors, Formats::ByteView bytes)
{
    Ios::Result<void> created = fileSystem.CreateFile(nandPath, EverybodyReadsAndWrites, NoAttributeFlags, CurrentCaller());
    if (created.Failed())
        throw MakeNandError("make", pathForErrors, created.Error());

    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(nandPath, Ios::OpenMode::Write, CurrentCaller());
    if (opened.Failed())
        throw MakeNandError("open", pathForErrors, opened.Error());

    Ios::Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        throw MakeNandError("write", pathForErrors, written.Error());
}

// The caller of the title that runs now, which changes when a program becomes another title.
Ios::Caller IosNandFiles::CurrentCaller() const
{
    if (titles != nullptr)
        return titles->GetCaller();

    return caller;
}

Ios::NandPath IosNandFiles::ParsePath(std::string_view path) const
{
    Ios::Result<Ios::NandPath> parsed = Ios::NandPath::Parse(path);
    if (parsed.Failed())
        throw MenuError(std::format("The path {} is not a path of the NAND", path));

    return parsed.Value();
}

}
