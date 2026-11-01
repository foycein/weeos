#include "SystemMenu/BackgroundServices/BootFiles.h"

#include "Formats/StateFile.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view StateFilePath = "/title/00000001/00000002/data/state.dat";
constexpr std::string_view BootCachePath = "/title/00000001/00000002/data/cache.dat";

// The boot stage writes this disc state while the menu runs, whatever the drive holds.
constexpr Formats::Byte DiscStateWhileTheMenuRuns = 2;

// The original fills the new file with one megabyte of memory at a time. WeeOS fills it with zeros.
constexpr std::size_t BootCacheWriteSize = 0x100000;

void MarkThatTheMenuRuns(NandFileAccess& files)
{
    Ios::NandPath path = MenuPath(StateFilePath);
    files.Create(path, EverybodyReadsAndWrites);

    Formats::StateFile state = {};
    state.type = Formats::StateTypeUnknown;
    state.discState = DiscStateWhileTheMenuRuns;
    state.returnTo = Formats::StateReturnToMenu;
    files.Write(path, Formats::WriteStateFile(state));
}

void DeleteBootCacheOfWrongSize(NandFileAccess& files, const Ios::NandPath& path)
{
    Ios::Result<std::uint32_t> length = files.Length(path);
    if (length.Failed())
        return;

    if (length.Value() != BootCacheSize)
        files.Delete(path);
}

void MakeBootCacheIfMissing(NandFileAccess& files, const Ios::NandPath& path)
{
    Ios::Result<void> created = files.Create(path, OthersHaveNoRights);
    if (created.Failed())
        return;

    Formats::Bytes zeros(BootCacheWriteSize, 0);
    for (std::size_t written = 0; written < BootCacheSize; written += BootCacheWriteSize)
        files.Append(path, zeros);
}

}

void PrepareBootFiles(NandFileAccess& files)
{
    MarkThatTheMenuRuns(files);

    Ios::NandPath bootCache = MenuPath(BootCachePath);
    DeleteBootCacheOfWrongSize(files, bootCache);
    MakeBootCacheIfMissing(files, bootCache);
}

}
