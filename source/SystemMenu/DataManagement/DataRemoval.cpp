#include "SystemMenu/DataManagement/DataRemoval.h"

#include "SystemMenu/DataManagement/BackupArchive.h"
#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/TitleKinds.h"

#include "SystemMenu/BackgroundServices/NandFileAccess.h"
#include "SystemMenu/BackgroundServices/TitleCleanup.h"

#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BannerFileName = "banner.bin";

// A title that holds nothing any more goes completely, and its title metadata goes into the backup file tmds.sys, so that
// its save data can come back from the SD card (0x8134DB7C: ESMisc CheckSafeDeleteTitle 0x81366758 and DeleteTitle 0x81366940).
void DeleteTitleWhenNothingIsLeft(DataManagementServices& services, Ios::TitleId titleId)
{
    NandFileAccess files(services.fileSystem, services.titles);
    TitleCleanup cleanup(files, services.titles);
    if (!cleanup.IsTitleEmpty(titleId))
        return;

    Require(cleanup.DeleteTitleKeepingBackup(titleId), std::format("delete the title {}", titleId.ToString()));
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
