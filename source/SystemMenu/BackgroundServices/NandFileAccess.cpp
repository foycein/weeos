#include "SystemMenu/BackgroundServices/NandFileAccess.h"

#include "SystemMenu/MenuError.h"

#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::uint8_t NoFlags = 0;

}

Ios::NandPath MenuPath(std::string_view text)
{
    Ios::Result<Ios::NandPath> path = Ios::NandPath::Parse(text);
    if (path.Failed())
        throw MenuError(std::format("The path {} is not a path of the NAND", text));

    return path.Value();
}

NandFileAccess::NandFileAccess(Ios::FileSystemService& fileSystemService, const Ios::TitleService& titleService)
    : fileSystem(fileSystemService)
    , titles(titleService)
{
}

Ios::Result<Formats::Bytes> NandFileAccess::Read(const Ios::NandPath& path)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(path, Ios::OpenMode::Read, titles.GetCaller());
    if (opened.Failed())
        return opened.Error();

    Ios::Result<Ios::FileStatistics> statistics = opened.Value().GetStatistics();
    if (statistics.Failed())
        return statistics.Error();

    return opened.Value().Read(statistics.Value().length);
}

Ios::Result<std::uint32_t> NandFileAccess::Length(const Ios::NandPath& path)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(path, Ios::OpenMode::Read, titles.GetCaller());
    if (opened.Failed())
        return opened.Error();

    Ios::Result<Ios::FileStatistics> statistics = opened.Value().GetStatistics();
    if (statistics.Failed())
        return statistics.Error();

    return statistics.Value().length;
}

Ios::Result<Ios::EntryAttributes> NandFileAccess::Attributes(const Ios::NandPath& path)
{
    return fileSystem.GetAttributes(path, titles.GetCaller());
}

Ios::Result<void> NandFileAccess::Create(const Ios::NandPath& path, const Ios::EntryPermissions& permissions)
{
    return fileSystem.CreateFile(path, permissions, NoFlags, titles.GetCaller());
}

Ios::Result<void> NandFileAccess::Write(const Ios::NandPath& path, Formats::ByteView bytes)
{
    return WriteAt(path, bytes, Ios::SeekOrigin::Start);
}

Ios::Result<void> NandFileAccess::Append(const Ios::NandPath& path, Formats::ByteView bytes)
{
    return WriteAt(path, bytes, Ios::SeekOrigin::End);
}

Ios::Result<void> NandFileAccess::Delete(const Ios::NandPath& path)
{
    return fileSystem.DeleteEntry(path, titles.GetCaller());
}

Ios::Result<std::vector<std::string>> NandFileAccess::List(const Ios::NandPath& directory)
{
    return fileSystem.ListDirectory(directory, titles.GetCaller());
}

Ios::FileSystemService& NandFileAccess::FileSystem()
{
    return fileSystem;
}

Ios::Result<void> NandFileAccess::WriteAt(const Ios::NandPath& path, Formats::ByteView bytes, Ios::SeekOrigin origin)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(path, Ios::OpenMode::Write, titles.GetCaller());
    if (opened.Failed())
        return opened.Error();

    Ios::Result<std::uint32_t> moved = opened.Value().Seek(0, origin);
    if (moved.Failed())
        return moved.Error();

    Ios::Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        return written.Error();

    return opened.Value().Close();
}

}
