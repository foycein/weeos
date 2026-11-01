#pragma once

#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/DataManagementServices.h"

#include "Formats/SdCardBackup.h"
#include "Formats/TitleMetadata.h"

#include <array>
#include <cstdint>
#include <vector>

namespace SystemMenu
{

// The backup archive that follows the header of data.bin and of content.bin, as the WAD library of the SDK writes and reads it
// (WADBackup 0x815C137C, WADImportEx 0x815C00A0). Its layout is in Formats/SdCardBackup.h.

// The save data of a title: every file and directory of its data folder, read as the title, the directories before what they hold.
// The directory "nocopy" at the top and the file banner.bin are left out (the banner goes into the header of data.bin).
// The data of the files is encrypted with the SD key. The transfer id is the one of this console (ReadTransferId).
Formats::Bytes WriteSaveArchive(DataManagementServices& services, Ios::TitleId titleId, const NandFiles& titleFiles,
    const std::array<Formats::Byte, Formats::TransferIdSize>& transferId);

// A channel: its title metadata and the contents that only it has, as the title service exports them.
Formats::Bytes WriteChannelArchive(DataManagementServices& services, Ios::TitleId titleId);

// A file or a directory of the save data in an archive. The data is still encrypted.
struct ArchiveFile
{
    Formats::BackupFileHeader header;
    Formats::ByteView encryptedData;
};

struct ArchiveContent
{
    Formats::ContentRecord record;
    Formats::ByteView encryptedData;
};

// An archive that was read from the SD card. The views point into the bytes it was read from.
struct BackupArchive
{
    Formats::BackupArchiveHeader header;
    Formats::ByteView titleMetadataBytes;
    std::vector<ArchiveContent> contents;
    std::vector<ArchiveFile> files;
};

// Throws a DataError (DamagedBackup) when the bytes are no archive.
BackupArchive ReadBackupArchive(Formats::ByteView bytes);

// Checks the signature at the end of the archive with the certificates of the console that made it. The console checks it only
// for an archive with files. Throws a DataError (DamagedBackup) when it does not fit.
void CheckArchiveSignature(const DataManagementServices& services, Formats::ByteView archiveBytes, const Formats::BackupArchiveHeader& header);

// A header of data.bin or content.bin, encrypted with the SD key of the console and the initialization vector of the SD card.
Formats::Bytes EncryptForCard(const Ios::ConsoleIdentityService& identity, Formats::ByteView plainBytes);
Formats::Bytes DecryptFromCard(const Ios::ConsoleIdentityService& identity, Formats::ByteView encryptedBytes);

// Gets the place of a new backup file ready: AlreadyThere when a file bigger than its header is there, a smaller one is deleted,
// and the folders above it are made.
void PrepareBackupFile(Ios::SdCardService& sdCard, const Ios::SdPath& file, std::size_t headerSize);

// The System Menu has looked at the files of a title as that title. From now on it is the System Menu again.
void BecomeSystemMenuAgain(Ios::TitleService& titles);

// The first bytes of /shared2/succession/transfer.id, or zeros when the console has no such file.
// Save data in a directory "notransfer" goes back only to the console with the same bytes.
std::array<Formats::Byte, Formats::TransferIdSize> ReadTransferId(const NandFiles& menuFiles);

}
