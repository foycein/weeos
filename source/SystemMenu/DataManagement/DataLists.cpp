#include "SystemMenu/DataManagement/DataLists.h"

#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/NandRoom.h"
#include "SystemMenu/DataManagement/SaveBackup.h"
#include "SystemMenu/DataManagement/TitleKinds.h"

#include "Formats/SdCardBackup.h"
#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <format>
#include <functional>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view CardTitlesFolder = "/private/wii/title";
constexpr std::size_t MostEntriesOnCard = 2400;

// An empty directory counts itself.
constexpr std::uint32_t InodesOfEmptyDirectory = 1;

std::uint32_t ClustersOfDirectory(const Ios::FileSystemService& fileSystem, const Ios::NandPath& directory)
{
    Ios::Result<Ios::DirectoryUsage> usage = fileSystem.GetDirectoryUsage(directory);
    if (usage.Failed())
        return 0;

    return usage.Value().usedClusters;
}

std::vector<Ios::TitleId> InstalledTitles(DataManagementServices& services)
{
    Ios::Result<std::vector<Ios::TitleId>> titles = services.titles.ListInstalledTitles();
    if (titles.Failed())
        return {};

    return titles.Value();
}

bool HasSaveData(const Ios::FileSystemService& fileSystem, Ios::TitleId titleId)
{
    Ios::Result<Ios::DirectoryUsage> usage = fileSystem.GetDirectoryUsage(Ios::DataFolderPath(titleId));
    return usage.Succeeded() && usage.Value().usedInodes > InodesOfEmptyDirectory;
}

using BackupPathOfCode = std::function<Ios::SdPath(std::uint32_t)>;

std::vector<std::uint32_t> ListBackupsOnCard(DataManagementServices& services, const BackupPathOfCode& pathOfCode, std::size_t headerSize)
{
    Ios::Result<std::vector<Ios::SdEntry>> entries = services.sdCard.ListDirectory(MakeSdPath(CardTitlesFolder));
    if (entries.Failed())
        return {};

    std::vector<std::uint32_t> codes;
    for (const Ios::SdEntry& entry : entries.Value())
    {
        std::optional<std::uint32_t> lowPart = LowPartOfBackupFolder(entry.name);
        if (entry.kind != Ios::EntryKind::Directory || !lowPart)
            continue;

        std::optional<std::uint64_t> size = SdFileSize(services.sdCard, pathOfCode(*lowPart));
        if (size && *size > headerSize)
            codes.push_back(*lowPart);

        if (codes.size() == MostEntriesOnCard)
            break;
    }
    return codes;
}

}

bool IsChannelComplete(const Ios::TitleService& titles, Ios::TitleId titleId)
{
    Ios::Result<Formats::TitleMetadata> metadata = titles.ReadTitleMetadata(titleId);
    Ios::Result<std::vector<std::uint32_t>> presentIds = titles.ListPresentContentIds(titleId);
    if (metadata.Failed() || presentIds.Failed() || presentIds.Value().empty())
        return false;

    for (const Formats::ContentRecord& record : metadata.Value().contents)
    {
        bool isOptional = Ios::IsOptionalContent(record);
        bool isPresent = std::find(presentIds.Value().begin(), presentIds.Value().end(), record.id) != presentIds.Value().end();
        if (!isPresent && !isOptional)
            return false;
    }
    return true;
}

std::vector<Ios::TitleId> ListNandChannels(DataManagementServices& services)
{
    std::vector<Ios::TitleId> channels;
    for (Ios::TitleId titleId : InstalledTitles(services))
    {
        if (IsChannelKind(titleId) && IsChannelComplete(services.titles, titleId))
            channels.push_back(titleId);
    }
    return channels;
}

std::vector<Ios::TitleId> ListNandSaveData(DataManagementServices& services)
{
    std::vector<Ios::TitleId> saves;
    for (Ios::TitleId titleId : InstalledTitles(services))
    {
        if (HasSaveDataKind(titleId) && HasSaveData(services.fileSystem, titleId))
            saves.push_back(titleId);
    }
    return saves;
}

std::vector<std::uint32_t> ListChannelsOnCard(DataManagementServices& services)
{
    return ListBackupsOnCard(services, ChannelBackupPath, Formats::ChannelBackupHeaderSize);
}

std::vector<std::uint32_t> ListSaveDataOnCard(DataManagementServices& services)
{
    return ListBackupsOnCard(services, SaveBackupPath, Formats::SaveBackupHeaderSize);
}

std::uint64_t NandChannelBytes(const DataManagementServices& services, Ios::TitleId titleId)
{
    std::uint32_t contentClusters = ClustersOfDirectory(services.fileSystem, Ios::ContentFolderPath(titleId));
    std::uint32_t metaClusters = ClustersOfDirectory(services.fileSystem, MetaFolderPath(titleId));
    return (contentClusters + metaClusters) * ClusterBytes;
}

std::uint64_t NandSaveDataBytes(const DataManagementServices& services, Ios::TitleId titleId)
{
    return ClustersOfDirectory(services.fileSystem, Ios::DataFolderPath(titleId)) * ClusterBytes;
}

std::optional<std::uint64_t> CardChannelBytes(const DataManagementServices& services, std::uint32_t lowPart)
{
    return SdFileSize(services.sdCard, ChannelBackupPath(lowPart));
}

std::optional<std::uint64_t> CardSaveDataBytes(const DataManagementServices& services, std::uint32_t lowPart)
{
    return SdFileSize(services.sdCard, SaveBackupPath(lowPart));
}

}
