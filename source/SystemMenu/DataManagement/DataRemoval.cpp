#include "SystemMenu/DataManagement/DataRemoval.h"

#include "SystemMenu/DataManagement/BackupArchive.h"
#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/TitleKinds.h"

#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BannerFileName = "banner.bin";
constexpr std::uint16_t SharedContentTypeFlag = 0x8000;

// The console deletes the whole title only when its version is below this (ESMisc 0x81366758).
constexpr std::uint16_t VersionOfTitlesThatGoWhole = 0x100;

constexpr std::uint32_t InodesOfEmptyDirectory = 1;

bool HasPrivateContents(const Ios::TitleService& titles, const Formats::TitleMetadata& metadata)
{
    Ios::Result<std::vector<std::uint32_t>> presentIds = titles.ListPresentContentIds(metadata);
    if (presentIds.Failed())
        return true;

    for (const Formats::ContentRecord& record : metadata.contents)
    {
        bool isShared = (record.type & SharedContentTypeFlag) != 0;
        bool isPresent = std::find(presentIds.Value().begin(), presentIds.Value().end(), record.id) != presentIds.Value().end();
        if (isPresent && !isShared)
            return true;
    }
    return false;
}

bool IsDataFolderEmpty(const Ios::FileSystemService& fileSystem, Ios::TitleId titleId)
{
    Ios::Result<Ios::DirectoryUsage> usage = fileSystem.GetDirectoryUsage(Ios::DataFolderPath(titleId));
    return usage.Failed() || usage.Value().usedInodes <= InodesOfEmptyDirectory;
}

// A title with no contents, no save data and an old version goes completely (ESMisc::CheckSafeDeleteTitle 0x81366758).
void DeleteTitleWhenNothingIsLeft(DataManagementServices& services, Ios::TitleId titleId)
{
    Ios::Result<Formats::TitleMetadata> metadata = services.titles.ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return;

    bool isOldVersion = metadata.Value().titleVersion < VersionOfTitlesThatGoWhole;
    if (!isOldVersion || HasPrivateContents(services.titles, metadata.Value()) || !IsDataFolderEmpty(services.fileSystem, titleId))
        return;

    Require(services.titles.DeleteTitle(titleId), std::format("delete the title {}", titleId.ToString()));
}

void EraseChannel(DataManagementServices& services, Ios::TitleId titleId)
{
    if (!IsChannelKind(titleId))
        throw DataError(DataResult::Failed, std::format("{} is no channel", titleId.ToString()));

    NandAccess menuFiles(services.fileSystem, services.titles.GetCaller());
    menuFiles.DeleteIfPresent(MetaFolderPath(titleId));
    Require(services.titles.DeleteTitleContents(titleId), std::format("delete the contents of {}", titleId.ToString()));
    DeleteTitleWhenNothingIsLeft(services, titleId);
}

void EraseSaveData(DataManagementServices& services, Ios::TitleId titleId)
{
    Require(services.titles.BecomeTitle(titleId), std::format("become the title {}", titleId.ToString()));
    NandAccess titleFiles(services.fileSystem, services.titles.GetCaller());

    Ios::NandPath dataFolder = Ios::DataFolderPath(titleId);
    titleFiles.DeleteIfPresent(ChildOf(dataFolder, BannerFileName));
    for (const std::string& name : titleFiles.List(dataFolder))
        titleFiles.DeleteIfPresent(ChildOf(dataFolder, name));

    BecomeSystemMenuAgain(services.titles);
    DeleteTitleWhenNothingIsLeft(services, titleId);
}

template <typename Work>
DataResult RunAndCatch(DataManagementServices& services, Work work)
{
    DataResult result = DataResult::Done;
    try
    {
        work();
    }
    catch (const DataError& error)
    {
        result = error.Result();
    }

    BecomeSystemMenuAgain(services.titles);
    return result;
}

}

DataResult EraseChannelFromNand(DataManagementServices& services, Ios::TitleId titleId)
{
    return RunAndCatch(services, [&] { EraseChannel(services, titleId); });
}

DataResult EraseSaveDataFromNand(DataManagementServices& services, Ios::TitleId titleId)
{
    return RunAndCatch(services, [&] { EraseSaveData(services, titleId); });
}

DataResult EraseChannelFromCard(DataManagementServices& services, std::uint32_t lowPart)
{
    return RunAndCatch(services, [&] { DeleteSdFileIfPresent(services.sdCard, ChannelBackupPath(lowPart)); });
}

DataResult EraseSaveDataFromCard(DataManagementServices& services, std::uint32_t lowPart)
{
    return RunAndCatch(services, [&] { DeleteSdFileIfPresent(services.sdCard, SaveBackupPath(lowPart)); });
}

DataResult MoveChannelToSdCard(DataManagementServices& services, Ios::TitleId titleId)
{
    DataResult copied = CopyChannelToSdCard(services, titleId);
    if (copied != DataResult::Done)
        return copied;

    return EraseChannelFromNand(services, titleId);
}

DataResult MoveChannelToNand(DataManagementServices& services, std::uint32_t lowPart)
{
    DataResult copied = CopyChannelToNand(services, lowPart);
    if (copied != DataResult::Done)
        return copied;

    return EraseChannelFromCard(services, lowPart);
}

SaveCopyReport MoveSaveDataToSdCard(DataManagementServices& services, Ios::TitleId titleId)
{
    SaveCopyReport report = CopySaveDataToSdCard(services, titleId);
    if (report.result != DataResult::Done)
        return report;

    report.result = EraseSaveDataFromNand(services, titleId);
    return report;
}

SaveRestoreReport MoveSaveDataToNand(DataManagementServices& services, std::uint32_t lowPart)
{
    SaveRestoreReport report = CopySaveDataToNand(services, lowPart);
    if (report.result != DataResult::Done || report.hasLeftOutNoTransferFiles)
        return report;

    report.result = EraseSaveDataFromCard(services, lowPart);
    return report;
}

}
