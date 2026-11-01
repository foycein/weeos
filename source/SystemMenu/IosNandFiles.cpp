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

MenuError MakeNandError(std::string_view action, std::string_view path, Ios::IosError error)
{
    std::string message = std::format("The menu cannot {} the file {} (result code {})", action, path, Ios::OriginalResultCode(error));
    return MenuError(message, FatalErrorOf(error));
}

}

IosNandFiles::IosNandFiles(Ios::FileSystemService& fileSystemService, const Ios::TitleService& titleService)
    : fileSystem(fileSystemService)
    , titles(titleService)
{
}

std::optional<Formats::Bytes> IosNandFiles::ReadFile(std::string_view path)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(ParsePath(path), Ios::OpenMode::Read, titles.GetCaller());
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
    Ios::Result<void> deleted = fileSystem.DeleteEntry(ParsePath(path), titles.GetCaller());
    if (deleted.Failed() && deleted.Error() != Ios::IosError::NotFound)
        throw MakeNandError("delete", path, deleted.Error());
}

void IosNandFiles::WriteFile(std::string_view path, Formats::ByteView bytes)
{
    DeleteFile(path);

    Ios::NandPath nandPath = ParsePath(path);
    Ios::Result<void> created = fileSystem.CreateFile(nandPath, EverybodyReadsAndWrites, NoAttributeFlags, titles.GetCaller());
    if (created.Failed())
        throw MakeNandError("make", path, created.Error());

    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(nandPath, Ios::OpenMode::Write, titles.GetCaller());
    if (opened.Failed())
        throw MakeNandError("open", path, opened.Error());

    Ios::Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        throw MakeNandError("write", path, written.Error());
}

Ios::NandPath IosNandFiles::ParsePath(std::string_view path) const
{
    Ios::Result<Ios::NandPath> parsed = Ios::NandPath::Parse(path);
    if (parsed.Failed())
        throw MenuError(std::format("The path {} is not a path of the NAND", path));

    return parsed.Value();
}

}
