#include "SystemMenu/DataManagement/StartCleanup.h"

#include "SystemMenu/BackgroundServices/NandFileAccess.h"
#include "SystemMenu/BackgroundServices/TitleCleanup.h"
#include "SystemMenu/DataManagement/BackupArchive.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/DataLists.h"
#include "SystemMenu/DataManagement/DataRemoval.h"
#include "SystemMenu/DataManagement/TitleKinds.h"

#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace SystemMenu
{

namespace
{

// The last letter of the code of a channel for every region (0x81353E84).
constexpr std::uint32_t LastLetterMask = 0xFF;
constexpr std::uint32_t EveryRegionLetter = 'A';

// The titles whose code starts with "UP" and a zero byte are never cleaned (0x8134A588).
constexpr std::uint32_t CodeWithoutLastLetterMask = 0xFFFFFF00;
constexpr std::uint32_t NeverCleanedCode = 0x55500000;

constexpr std::string_view BannerFileName = "banner.bin";
constexpr std::string_view NoEraseFolderName = "noerase";

// How much of a channel is on the NAND (0x81351BB0): nothing, a part, or every content that is not optional.
enum class ContentState
{
    Nothing,
    Partial,
    Complete,
};

// The contents of the title's own content folder; shared contents live elsewhere and do not count.
bool HasOwnContents(const Ios::TitleService& titles, Ios::TitleId titleId)
{
    Ios::Result<Formats::TitleMetadata> metadata = titles.ReadTitleMetadata(titleId);
    Ios::Result<std::vector<std::uint32_t>> presentIds = titles.ListPresentContentIds(titleId);
    if (metadata.Failed() || presentIds.Failed())
        return false;

    for (const Formats::ContentRecord& record : metadata.Value().contents)
    {
        bool isShared = Ios::IsSharedContent(record);
        bool isPresent = std::find(presentIds.Value().begin(), presentIds.Value().end(), record.id) != presentIds.Value().end();
        if (!isShared && isPresent)
            return true;
    }
    return false;
}

ContentState ContentStateOf(const Ios::TitleService& titles, Ios::TitleId titleId)
{
    if (!HasOwnContents(titles, titleId))
        return ContentState::Nothing;

    if (IsChannelComplete(titles, titleId))
        return ContentState::Complete;

    return ContentState::Partial;
}

std::vector<Ios::TitleId> InstalledTitles(const Ios::TitleService& titles)
{
    Ios::Result<std::vector<Ios::TitleId>> installed = titles.ListInstalledTitles();
    if (installed.Failed())
        return {};

    return installed.Value();
}

bool IsCopyForEveryRegion(Ios::TitleId titleId)
{
    return (titleId.lowPart & LastLetterMask) == EveryRegionLetter;
}

// The same channel in another region: the same family and the same code but for the last letter (0x81353E9C).
bool IsTheSameChannelOfAnotherRegion(Ios::TitleId titleId, Ios::TitleId otherTitleId)
{
    bool isSameFamily = titleId.highPart == otherTitleId.highPart;
    bool isSameCode = (titleId.lowPart & CodeWithoutLastLetterMask) == (otherTitleId.lowPart & CodeWithoutLastLetterMask);
    return otherTitleId != titleId && isSameFamily && isSameCode;
}

bool HasCopyOfAnotherRegion(const Ios::TitleService& titles, const std::vector<Ios::TitleId>& installed, Ios::TitleId titleId)
{
    for (Ios::TitleId otherTitleId : installed)
    {
        if (!IsTheSameChannelOfAnotherRegion(titleId, otherTitleId))
            continue;

        if (ContentStateOf(titles, otherTitleId) != ContentState::Nothing)
            return true;
    }
    return false;
}

void DeleteTickets(Ios::TitleService& titles, Ios::TitleId titleId)
{
    Ios::Result<std::vector<Ios::TicketView>> views = titles.ListTicketViews(titleId);
    if (views.Failed())
        return;

    for (const Ios::TicketView& view : views.Value())
        titles.DeleteTicket(view);
}

// 0x8134A380: the copy for every region gives way to the copy of a region.
void DeleteCopiesForEveryRegion(DataManagementServices& services)
{
    std::vector<Ios::TitleId> installed = InstalledTitles(services.titles);
    for (Ios::TitleId titleId : installed)
    {
        bool isCandidate = IsChannelKind(titleId) && IsCopyForEveryRegion(titleId);
        if (!isCandidate || ContentStateOf(services.titles, titleId) == ContentState::Nothing)
            continue;

        if (!HasCopyOfAnotherRegion(services.titles, installed, titleId))
            continue;

        DeleteTickets(services.titles, titleId);
        EraseChannelFromNand(services, titleId);
        EraseSaveDataFromNand(services, titleId);
    }
}

// 0x8134A588: channels that a broken copy or move left behind.
void CleanPartialChannels(DataManagementServices& services)
{
    NandFileAccess files(services.fileSystem, services.titles);
    TitleCleanup cleanup(files, services.titles);

    for (Ios::TitleId titleId : InstalledTitles(services.titles))
    {
        bool isNeverCleaned = (titleId.lowPart & CodeWithoutLastLetterMask) == NeverCleanedCode;
        if (!IsChannelKind(titleId) || isNeverCleaned)
            continue;

        ContentState state = ContentStateOf(services.titles, titleId);
        if (state == ContentState::Nothing && cleanup.IsTitleEmpty(titleId))
            cleanup.DeleteTitleKeepingBackup(titleId);
        else if (state == ContentState::Partial)
            EraseChannelFromNand(services, titleId);
    }
}

// The data folder has files but no banner.bin (0x8134AF68), and no folder noerase protects it.
bool IsSaveDataWithoutBanner(DataManagementServices& services, Ios::TitleId titleId)
{
    if (services.titles.BecomeTitle(titleId).Failed())
        return false;

    NandAccess titleFiles(services.fileSystem, services.titles.GetCaller());
    Ios::NandPath dataFolder = Ios::DataFolderPath(titleId);
    Ios::NandPath noEraseFolder = ChildOf(dataFolder, NoEraseFolderName);

    bool hasFiles = titleFiles.Exists(dataFolder) && !titleFiles.List(dataFolder).empty();
    bool hasBanner = titleFiles.Exists(ChildOf(dataFolder, BannerFileName));
    bool isProtected = titleFiles.Exists(noEraseFolder) && titleFiles.KindOf(noEraseFolder) == Ios::EntryKind::Directory;
    BecomeSystemMenuAgain(services.titles);

    return hasFiles && !hasBanner && !isProtected;
}

// 0x8134A718: save data that a broken copy left without its banner.
void CleanPartialSaveData(DataManagementServices& services)
{
    for (Ios::TitleId titleId : InstalledTitles(services.titles))
    {
        if (HasSaveDataKind(titleId) && IsSaveDataWithoutBanner(services, titleId))
            EraseSaveDataFromNand(services, titleId);
    }
}

}

void CleanUpBeforeTheList(DataManagementServices& services, DataKind kind)
{
    DeleteCopiesForEveryRegion(services);
    if (kind == DataKind::Channels)
        CleanPartialChannels(services);
    else
        CleanPartialSaveData(services);
}

}
