#include "SystemMenu/DataManagement/SaveBackup.h"

#include "SystemMenu/BackgroundServices/NandFileAccess.h"
#include "SystemMenu/BackgroundServices/TitleCleanup.h"
#include "SystemMenu/DataManagement/BackupArchive.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/NandRoom.h"
#include "SystemMenu/DataManagement/TitleKinds.h"

#include "Formats/FormatError.h"
#include "Formats/SaveBanner.h"
#include "Ios/Title/TitlePaths.h"

#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BannerFileName = "banner.bin";
constexpr std::string_view NoCopyFolderName = "nocopy";
constexpr std::string_view NoTransferFolderName = "notransfer";
constexpr std::string_view SaveBackupFileName = "data.bin";

// The folder in which the files of the save data are put together before they move into the data folder of the title.
constexpr std::string_view WorkFolderPath = "/tmp/nandsdwork";
constexpr Ios::EntryPermissions WorkFolderPermissions = {Ios::AccessRights::ReadWrite, Ios::AccessRights::None, Ios::AccessRights::None};
constexpr std::uint8_t NoFlags = 0;

Ios::NandPath BannerPath(Ios::TitleId titleId)
{
    return ChildOf(Ios::DataFolderPath(titleId), BannerFileName);
}

bool HasEntriesIn(const NandAccess& titleFiles, const Ios::NandPath& dataFolder, std::string_view folderName)
{
    Ios::NandPath folder = ChildOf(dataFolder, folderName);
    if (!titleFiles.Exists(folder) || titleFiles.KindOf(folder) != Ios::EntryKind::Directory)
        return false;

    return !titleFiles.List(folder).empty();
}

Formats::SaveBanner ReadBannerOrFail(Formats::ByteView banner)
{
    try
    {
        return Formats::ReadSaveBanner(banner);
    }
    catch (const Formats::FormatError& error)
    {
        throw DataError(DataResult::Failed, error.what());
    }
}

// The header goes encrypted before the archive. A file that could not be written whole is deleted again.
void WriteBackupFile(DataManagementServices& services, const Ios::SdPath& file, const Formats::SaveBackupHeader& header, Formats::ByteView archive)
{
    Formats::Bytes backup = EncryptForCard(services.identity, Formats::WriteSaveBackupHeader(header));
    backup.insert(backup.end(), archive.begin(), archive.end());
    try
    {
        WriteSdFile(services.sdCard, file, backup);
    }
    catch (const DataError&)
    {
        DeleteSdFileIfPresent(services.sdCard, file);
        throw;
    }
}

SaveCopyReport CopyToCard(DataManagementServices& services, Ios::TitleId titleId)
{
    Ios::SdPath file = SaveBackupPath(titleId.lowPart);
    PrepareBackupFile(services.sdCard, file, Formats::SaveBackupHeaderSize);

    NandAccess menuFiles(services.fileSystem, services.titles.GetCaller());
    std::array<Formats::Byte, Formats::TransferIdSize> transferId = ReadTransferId(menuFiles);

    Require(services.titles.BecomeTitle(titleId), std::format("become the title {}", titleId.ToString()));
    NandAccess titleFiles(services.fileSystem, services.titles.GetCaller());

    Formats::Bytes banner = titleFiles.ReadFile(BannerPath(titleId));
    Formats::SaveBanner parsedBanner = ReadBannerOrFail(banner);
    if ((parsedBanner.flags & Formats::SaveBannerNoCopyFlag) != 0)
        throw DataError(DataResult::MayNotBeCopied, "the banner says that the save data must not be copied");

    Formats::Bytes archive = WriteSaveArchive(services, titleId, titleFiles, transferId);
    if (Formats::ReadBackupArchiveHeader(archive).fileCount == 0)
        throw DataError(DataResult::MayNotBeCopied, "the save data has nothing that may be copied");

    std::uint64_t freeBytes = RequireOnCard(services.sdCard.GetFreeBytes(), "measure the room on the SD card");
    if (freeBytes < archive.size() + Formats::SaveBackupHeaderSize)
        throw DataError(DataResult::NotEnoughRoom, "the SD card is full");

    Ios::EntryAttributes bannerAttributes = titleFiles.AttributesOf(BannerPath(titleId));
    Formats::SaveBackupHeader header;
    header.titleId = titleId.ToValue();
    header.bannerPermissions = PermissionsToByte(bannerAttributes.permissions);
    header.bannerFlags = bannerAttributes.flags;
    header.banner = banner;
    WriteBackupFile(services, file, header, archive);

    Ios::NandPath dataFolder = Ios::DataFolderPath(titleId);
    SaveCopyReport report;
    report.hasNoCopyFiles = HasEntriesIn(titleFiles, dataFolder, NoCopyFolderName);
    report.hasNoTransferFiles = HasEntriesIn(titleFiles, dataFolder, NoTransferFolderName);
    return report;
}

// What the console takes from an archive: never the files of the directory "nocopy", and the files of "notransfer" only when the
// archive was made by this console.
bool IsFileTaken(const ArchiveFile& file, const Formats::BackupArchiveHeader& header, const std::array<Formats::Byte, Formats::TransferIdSize>& transferId)
{
    const std::string& name = file.header.name;
    if (name.starts_with(NoCopyFolderName))
        return false;

    if (name.starts_with(NoTransferFolderName))
        return header.transferId == transferId;

    return true;
}

NandRoomNeeded RoomNeededFor(const std::vector<const ArchiveFile*>& files, std::size_t bannerSize)
{
    // The files and banner.bin.
    NandRoomNeeded needed;
    needed.userInodes = static_cast<std::uint32_t>(files.size()) + 1;
    needed.userClusters = ClustersOfBytes(bannerSize);
    for (const ArchiveFile* file : files)
        needed.userClusters += ClustersOfBytes(file->header.size);

    return needed;
}

Formats::Bytes DecryptFile(const Ios::ConsoleIdentityService& identity, const ArchiveFile& file)
{
    Ios::Result<Ios::CipherResult> decrypted =
        identity.Decrypt(Ios::ConsoleIdentityService::SdCardKeyNumber, file.header.initializationVector, file.encryptedData);
    if (decrypted.Failed())
        throw DataError(DataResult::DamagedBackup, std::format("the file {} cannot be decrypted", file.header.name));

    Formats::Bytes data = decrypted.Value().data;
    data.resize(file.header.size);
    return data;
}

// The owner always gets the right to write while the files are made, and the rights of the archive afterwards.
Ios::EntryPermissions PermissionsWhileWriting(std::uint8_t permissionsByte)
{
    Ios::EntryPermissions permissions = PermissionsFromByte(permissionsByte);
    permissions.owner = Ios::AccessRights::ReadWrite;
    return permissions;
}

Ios::NandPath PathInWorkFolder(const Ios::NandPath& workFolder, const ArchiveFile& file)
{
    return MakeNandPath(std::format("{}/{}", workFolder.ToString(), file.header.name));
}

void PutFilesTogether(const DataManagementServices& services, NandAccess& titleFiles, const Ios::NandPath& workFolder,
    const std::vector<const ArchiveFile*>& files)
{
    for (const ArchiveFile* file : files)
    {
        Ios::NandPath path = PathInWorkFolder(workFolder, *file);
        Ios::EntryPermissions permissions = PermissionsWhileWriting(file->header.permissions);

        if (file->header.kind == Formats::BackupEntryKind::Directory)
            titleFiles.CreateDirectory(path, permissions, file->header.flags);
        else
            titleFiles.WriteNewFile(path, DecryptFile(services.identity, *file), permissions, file->header.flags);
    }

    for (const ArchiveFile* file : files)
        titleFiles.SetPermissions(PathInWorkFolder(workFolder, *file), PermissionsFromByte(file->header.permissions));
}

void MoveIntoDataFolder(NandAccess& titleFiles, const Ios::NandPath& workFolder, const Ios::NandPath& dataFolder)
{
    for (const std::string& name : titleFiles.List(workFolder))
        titleFiles.Move(ChildOf(workFolder, name), ChildOf(dataFolder, name));
}

// The file must be longer than its header.
Formats::SaveBackupHeader ReadHeaderOrFail(const DataManagementServices& services, Formats::ByteView backupStart, std::uint64_t fileSize,
    std::uint32_t lowPart)
{
    if (fileSize <= Formats::SaveBackupHeaderSize || backupStart.size() < Formats::SaveBackupHeaderSize)
        throw DataError(DataResult::DamagedBackup, "data.bin is not longer than its header");

    Formats::Bytes plainHeader = DecryptFromCard(services.identity, backupStart.first(Formats::SaveBackupHeaderSize));
    Formats::SaveBackupHeader header;
    try
    {
        header = Formats::ReadSaveBackupHeader(plainHeader);
    }
    catch (const Formats::FormatError& error)
    {
        throw DataError(DataResult::DamagedBackup, error.what());
    }

    if (Ios::TitleId::FromValue(header.titleId).lowPart != lowPart)
        throw DataError(DataResult::DamagedBackup, "data.bin belongs to another title");

    return header;
}

// A title that Data Management deleted with its save data comes back from the backup file tmds.sys (0x8134EDBC calls ESMisc
// PrepareTitleDir 0x813670A0). Without a backup the game was never played on this console.
void RestoreTitleOfSaveData(DataManagementServices& services, Ios::TitleId titleId)
{
    NandFileAccess files(services.fileSystem, services.titles);
    TitleCleanup cleanup(files, services.titles);
    Ios::Result<void> restored = cleanup.RestoreTitleMetadata(titleId);
    if (restored.Failed() && restored.Error() == Ios::IosError::NotFound)
        throw DataError(DataResult::TitleNotInstalled, std::format("the title {} is not on the NAND", titleId.ToString()));

    Require(restored, std::format("set up the title {} again", titleId.ToString()));
}

std::vector<const ArchiveFile*> FilesTakenFrom(const BackupArchive& archive, const std::array<Formats::Byte, Formats::TransferIdSize>& transferId)
{
    std::vector<const ArchiveFile*> takenFiles;
    for (const ArchiveFile& file : archive.files)
    {
        if (IsFileTaken(file, archive.header, transferId))
            takenFiles.push_back(&file);
    }
    return takenFiles;
}

// The files are put together in the work folder first, so that the data folder gets all of them or none.
void WriteIntoDataFolder(const DataManagementServices& services, NandAccess& titleFiles, Ios::TitleId titleId,
    const Formats::SaveBackupHeader& header, const std::vector<const ArchiveFile*>& takenFiles)
{
    Ios::NandPath workFolder = MakeNandPath(WorkFolderPath);
    titleFiles.DeleteIfPresent(workFolder);
    titleFiles.CreateDirectory(workFolder, WorkFolderPermissions, NoFlags);
    try
    {
        PutFilesTogether(services, titleFiles, workFolder, takenFiles);
        titleFiles.WriteNewFile(ChildOf(workFolder, BannerFileName), header.banner, PermissionsFromByte(header.bannerPermissions), header.bannerFlags);
        MoveIntoDataFolder(titleFiles, workFolder, Ios::DataFolderPath(titleId));
    }
    catch (const DataError&)
    {
        titleFiles.DeleteIfPresent(workFolder);
        throw;
    }
    titleFiles.DeleteIfPresent(workFolder);
}

SaveRestoreReport CopyToNand(DataManagementServices& services, std::uint32_t lowPart)
{
    Formats::Bytes backup = ReadSdFile(services.sdCard, SaveBackupPath(lowPart));
    Formats::SaveBackupHeader header = ReadHeaderOrFail(services, backup, backup.size(), lowPart);

    Formats::ByteView archiveBytes = Formats::ByteView(backup).subspan(Formats::SaveBackupHeaderSize);
    BackupArchive archive = ReadBackupArchive(archiveBytes);
    if (archive.header.titleId != header.titleId)
        throw DataError(DataResult::DamagedBackup, "the archive belongs to another title than the header");

    NandAccess menuFiles(services.fileSystem, services.titles.GetCaller());
    std::array<Formats::Byte, Formats::TransferIdSize> transferId = ReadTransferId(menuFiles);

    Ios::TitleId titleId = Ios::TitleId::FromValue(header.titleId);
    RestoreTitleOfSaveData(services, titleId);
    if (services.titles.BecomeTitle(titleId).Failed())
        throw DataError(DataResult::TitleNotInstalled, std::format("the title {} is not on the NAND", titleId.ToString()));

    NandAccess titleFiles(services.fileSystem, services.titles.GetCaller());
    if (titleFiles.Exists(BannerPath(titleId)))
        throw DataError(DataResult::AlreadyThere, "the NAND has save data of the title");

    std::vector<const ArchiveFile*> takenFiles = FilesTakenFrom(archive, transferId);
    if (!HasRoomFor(MeasureNandRoom(services.fileSystem), RoomNeededFor(takenFiles, header.banner.size())))
        throw DataError(DataResult::NotEnoughRoom, "the NAND is full");

    if (takenFiles.empty() && !archive.files.empty())
        throw DataError(DataResult::FromAnotherConsole, "every file may only go back to the console that made the copy");

    if (archive.header.filesSize != 0)
        CheckArchiveSignature(services, archiveBytes, archive.header);

    WriteIntoDataFolder(services, titleFiles, titleId, header, takenFiles);

    // An empty directory "notransfer" that stayed behind is no file that was left out.
    SaveRestoreReport report;
    report.hasLeftOutNoTransferFiles = takenFiles.size() + 1 < archive.files.size();
    return report;
}

}

Ios::SdPath SaveBackupPath(std::uint32_t lowPart)
{
    return MakeSdPath(std::format("/private/wii/title/{}/{}", BackupFolderName(lowPart), SaveBackupFileName));
}

SaveCopyReport CopySaveDataToSdCard(DataManagementServices& services, Ios::TitleId titleId)
{
    SaveCopyReport report;
    try
    {
        report = CopyToCard(services, titleId);
    }
    catch (const DataError& error)
    {
        report.result = error.Result();
    }

    BecomeSystemMenuAgain(services.titles);
    return report;
}

SaveRestoreReport CopySaveDataToNand(DataManagementServices& services, std::uint32_t lowPart)
{
    SaveRestoreReport report;
    try
    {
        report = CopyToNand(services, lowPart);
    }
    catch (const DataError& error)
    {
        report.result = error.Result();
    }

    BecomeSystemMenuAgain(services.titles);
    return report;
}

std::optional<Formats::SaveBackupHeader> ReadSaveBackupHeaderOnCard(DataManagementServices& services, std::uint32_t lowPart)
{
    Ios::SdPath file = SaveBackupPath(lowPart);
    std::optional<std::uint64_t> fileSize = SdFileSize(services.sdCard, file);
    if (!fileSize)
        return std::nullopt;

    try
    {
        Formats::Bytes backupStart = ReadSdFileStart(services.sdCard, file, Formats::SaveBackupHeaderSize);
        return ReadHeaderOrFail(services, backupStart, *fileSize, lowPart);
    }
    catch (const DataError&)
    {
        return std::nullopt;
    }
}

}
