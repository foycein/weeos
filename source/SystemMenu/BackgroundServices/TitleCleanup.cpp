#include "SystemMenu/BackgroundServices/TitleCleanup.h"

#include "Formats/FormatError.h"
#include "Formats/TitleMetadataBackupFile.h"
#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <array>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BackupFilePath = "/title/00000001/00000002/data/tmds.sys";

// The content folder of a title always holds itself and the title metadata. More entries are contents of the title.
constexpr std::uint32_t InodesOfAContentFolderWithoutContents = 2;

// A title with a version of 256 or more is never counted as empty.
constexpr std::uint16_t LowestVersionThatIsKept = 0x100;

// A channel whose name starts with these three letters is deleted whole when the user deletes it, even when it is not empty.
constexpr std::uint32_t AlwaysDeletedChannelFamily = 0x48414F00;
constexpr std::uint32_t ChannelFamilyMask = 0xFFFFFF00;

// The titles that InitSavedata deletes: the hidden titles DISC, DISK and DVDX and the downloaded titles JODI and HAXX,
// all of them used only by homebrew programs.
constexpr std::array<Ios::TitleId, 5> BlockedTitles = {{
    {0x00010008, 0x44495343},
    {0x00010008, 0x4449534B},
    {0x00010008, 0x44564458},
    {0x00010001, 0x4A4F4449},
    {0x00010001, 0x48415858},
}};

Ios::NandPath MetaFilePath(Ios::TitleId titleId)
{
    return MenuPath(std::format("/meta/{:08x}/{:08x}/title.met", titleId.highPart, titleId.lowPart));
}

bool IsFoundOrMissing(const Ios::Result<void>& result)
{
    return result.Succeeded() || result.Error() == Ios::IosError::NotFound;
}

}

TitleCleanup::TitleCleanup(NandFileAccess& nandFiles, Ios::TitleService& titleService)
    : files(nandFiles)
    , titles(titleService)
{
}

Ios::Result<void> TitleCleanup::DeleteTitleKeepingBackup(Ios::TitleId titleId)
{
    if (!FindBackup(titleId))
        AddBackup(titleId);

    return titles.DeleteTitle(titleId);
}

bool TitleCleanup::IsTitleEmpty(Ios::TitleId titleId)
{
    if (HasContentFiles(titleId))
        return false;

    if (CountOwnContentsOnNand(titleId) != 0)
        return false;

    Ios::Result<Ios::TitleMetadataView> view = titles.ReadTitleMetadataView(titleId);
    if (view.Failed())
        return false;

    if (view.Value().titleVersion >= LowestVersionThatIsKept)
        return false;

    return IsDataFolderEmpty(titleId);
}

void TitleCleanup::DeleteEmptyTitlesOnce()
{
    Ios::NandPath backupFile = MenuPath(BackupFilePath);
    if (files.Attributes(backupFile).Succeeded())
        return;

    Ios::Result<std::vector<Ios::TitleId>> installedTitles = titles.ListInstalledTitles();
    if (installedTitles.Succeeded())
    {
        for (Ios::TitleId titleId : installedTitles.Value())
        {
            if (titleId.highPart != Ios::SystemTitleHighPart && IsTitleEmpty(titleId))
                DeleteTitleKeepingBackup(titleId);
        }
    }

    files.Create(backupFile, EverybodyReadsAndWrites);
}

Ios::Result<void> TitleCleanup::RestoreTitleMetadata(Ios::TitleId titleId)
{
    Ios::Result<Ios::TitleMetadataView> view = titles.ReadTitleMetadataView(titleId);
    if (view.Succeeded())
        return Ios::Result<void>();

    if (view.Error() != Ios::IosError::NotFound)
        return view.Error();

    std::optional<Formats::TitleMetadataBackup> backup = FindBackup(titleId);
    if (!backup)
        return Ios::IosError::NotFound;

    Ios::Result<void> started = titles.BeginInstallation(backup->titleMetadata, {});
    if (started.Failed())
        return started;

    return titles.FinishInstallation();
}

Ios::Result<void> TitleCleanup::DeleteChannel(Ios::TitleId titleId)
{
    Ios::Result<void> metaDeleted = files.Delete(MetaFilePath(titleId));
    if (!IsFoundOrMissing(metaDeleted))
        return metaDeleted;

    bool isAlwaysDeleted = (titleId.lowPart & ChannelFamilyMask) == AlwaysDeletedChannelFamily;
    if (isAlwaysDeleted || IsTitleEmpty(titleId))
        return DeleteTitleKeepingBackup(titleId);

    return titles.DeleteTitleContents(titleId);
}

void TitleCleanup::DeleteBlockedTitles()
{
    Ios::Result<std::vector<Ios::TitleId>> installedTitles = titles.ListInstalledTitles();
    if (installedTitles.Failed())
        return;

    for (Ios::TitleId titleId : installedTitles.Value())
    {
        bool isBlocked = std::find(BlockedTitles.begin(), BlockedTitles.end(), titleId) != BlockedTitles.end();
        if (isBlocked)
            DeleteWithTickets(titleId);
    }
}

bool TitleCleanup::HasContentFiles(Ios::TitleId titleId)
{
    Ios::Result<Ios::DirectoryUsage> usage = files.FileSystem().GetDirectoryUsage(Ios::ContentFolderPath(titleId));
    if (usage.Failed())
        return false;

    return usage.Value().usedInodes > InodesOfAContentFolderWithoutContents;
}

int TitleCleanup::CountOwnContentsOnNand(Ios::TitleId titleId)
{
    Ios::Result<Formats::TitleMetadata> metadata = titles.ReadTitleMetadata(titleId);
    Ios::Result<std::vector<std::uint32_t>> presentIds = titles.ListPresentContentIds(titleId);
    if (metadata.Failed() || presentIds.Failed())
        return 0;

    int count = 0;
    for (const Formats::ContentRecord& record : metadata.Value().contents)
    {
        const std::vector<std::uint32_t>& ids = presentIds.Value();
        bool isPresent = std::find(ids.begin(), ids.end(), record.id) != ids.end();
        if (isPresent && !Ios::IsSharedContent(record))
            ++count;
    }
    return count;
}

// The data folder belongs to the title, so the menu looks into it as that title for a moment.
bool TitleCleanup::IsDataFolderEmpty(Ios::TitleId titleId)
{
    if (titles.BecomeTitle(titleId).Failed())
        return false;

    Ios::Result<std::vector<std::string>> names = files.List(Ios::DataFolderPath(titleId));
    titles.BecomeTitle(Ios::SystemMenuTitleId);

    return names.Succeeded() && names.Value().empty();
}

// A damaged backup file counts as one without the title.
std::optional<Formats::TitleMetadataBackup> TitleCleanup::FindBackup(Ios::TitleId titleId)
{
    Ios::Result<Formats::Bytes> backupFile = files.Read(MenuPath(BackupFilePath));
    if (backupFile.Failed())
        return std::nullopt;

    try
    {
        for (const Formats::TitleMetadataBackup& backup : Formats::ReadTitleMetadataBackups(backupFile.Value()))
        {
            if (backup.titleId == titleId.ToValue())
                return backup;
        }
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
    return std::nullopt;
}

// The file only ever grows by whole records. A file of another length is not touched, as in the original.
Ios::Result<void> TitleCleanup::AddBackup(Ios::TitleId titleId)
{
    Ios::NandPath backupFile = MenuPath(BackupFilePath);
    files.Create(backupFile, EverybodyReadsAndWrites);

    Ios::Result<std::uint32_t> length = files.Length(backupFile);
    if (length.Failed())
        return length.Error();

    if (length.Value() % Formats::TitleMetadataBackupAlignment != 0)
        return Ios::IosError::InvalidArgument;

    Ios::Result<Formats::Bytes> titleMetadata = titles.ReadTitleMetadataBytes(titleId);
    if (titleMetadata.Failed())
        return titleMetadata.Error();

    Formats::TitleMetadataBackup backup = {titleId.ToValue(), titleMetadata.Value()};
    return files.Append(backupFile, Formats::WriteTitleMetadataBackup(backup));
}

void TitleCleanup::DeleteWithTickets(Ios::TitleId titleId)
{
    titles.DeleteTitle(titleId);

    Ios::Result<std::vector<Ios::TicketView>> views = titles.ListTicketViews(titleId);
    if (views.Failed())
        return;

    for (const Ios::TicketView& view : views.Value())
        titles.DeleteTicket(view);
}

}
