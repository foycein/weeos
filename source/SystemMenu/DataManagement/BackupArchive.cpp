#include "SystemMenu/DataManagement/BackupArchive.h"

#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/Sha1.h"
#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view NoCopyFolder = "nocopy";
constexpr std::string_view BannerFile = "banner.bin";
constexpr std::string_view TransferIdPath = "/shared2/succession/transfer.id";

// The data of a file is encrypted in whole pieces of 32 bytes.
constexpr std::size_t FileEncryptionAlignment = 0x20;
constexpr std::size_t ContentEncryptionAlignment = Formats::AesBlockSize;

constexpr std::size_t SignatureSize = 0x3C;
constexpr std::size_t SignedSizeAlignment = 0x20;

// A file or a directory of the save data, read from the NAND.
struct SaveEntry
{
    std::string name;
    Ios::EntryKind kind;
    Ios::EntryAttributes attributes;
    Formats::Bytes data;
};

void Append(Formats::Bytes& archive, Formats::ByteView bytes)
{
    archive.insert(archive.end(), bytes.begin(), bytes.end());
}

void FillToAlignment(Formats::Bytes& archive, Formats::BackupFillNumbers& fill)
{
    std::size_t alignedSize = Formats::RoundUpToMultiple(archive.size(), Formats::BackupAlignment);
    Append(archive, fill.NextBytes(alignedSize - archive.size()));
}

std::uint32_t AlignedSize(std::uint64_t size)
{
    return static_cast<std::uint32_t>(Formats::RoundUpToMultiple(size, Formats::BackupAlignment));
}

bool IsTopLevelEntryLeftOut(std::string_view name)
{
    return name == NoCopyFolder || name == BannerFile;
}

void CollectEntries(const NandAccess& files, const Ios::NandPath& folder, const std::string& namePrefix, std::vector<SaveEntry>& entries)
{
    bool isTopLevel = namePrefix.empty();
    for (const std::string& name : files.List(folder))
    {
        if (isTopLevel && IsTopLevelEntryLeftOut(name))
            continue;

        Ios::NandPath path = ChildOf(folder, name);
        SaveEntry entry;
        entry.name = namePrefix + name;
        entry.kind = files.KindOf(path);
        entry.attributes = files.AttributesOf(path);
        if (entry.kind == Ios::EntryKind::File)
            entry.data = files.ReadFile(path);

        entries.push_back(entry);

        if (entry.kind == Ios::EntryKind::Directory)
            CollectEntries(files, path, entry.name + "/", entries);
    }
}

void AppendFile(Formats::Bytes& archive, const SaveEntry& entry, const DataManagementServices& services, Formats::BackupFillNumbers& fill)
{
    Formats::BackupFileHeader header;
    header.size = static_cast<std::uint32_t>(entry.data.size());
    header.permissions = PermissionsToByte(entry.attributes.permissions);
    header.flags = entry.attributes.flags;
    header.kind = entry.kind == Ios::EntryKind::File ? Formats::BackupEntryKind::File : Formats::BackupEntryKind::Directory;
    header.name = entry.name;

    Formats::Bytes headerFill = fill.NextBytes(Formats::BackupFileHeaderSize);
    Formats::Bytes initializationVector = fill.NextBytes(Formats::AesBlockSize);
    std::copy(initializationVector.begin(), initializationVector.end(), header.initializationVector.begin());

    try
    {
        Append(archive, Formats::WriteBackupFileHeader(header, headerFill));
    }
    catch (const Formats::FormatError& error)
    {
        throw DataError(DataResult::Failed, error.what());
    }

    if (entry.data.empty())
        return;

    Formats::Bytes plainData = entry.data;
    plainData.resize(Formats::RoundUpToMultiple(plainData.size(), FileEncryptionAlignment), 0);
    Ios::CipherResult encrypted = Require(
        services.identity.Encrypt(Ios::ConsoleIdentityService::SdCardKeyNumber, header.initializationVector, plainData),
        std::format("encrypt {}", entry.name));
    Append(archive, encrypted.data);
    FillToAlignment(archive, fill);
}

// Puts the parts together behind the header, then signs everything with the key of the console.
Formats::Bytes AssembleArchive(const DataManagementServices& services, Formats::BackupArchiveHeader header, Formats::ByteView titleMetadata,
    const std::vector<Formats::Bytes>& contents, const std::vector<SaveEntry>& entries, std::uint32_t fillSeed)
{
    header.deviceId = services.identity.GetDeviceId();
    header.titleMetadataSize = static_cast<std::uint32_t>(titleMetadata.size());
    header.fileCount = static_cast<std::uint32_t>(entries.size());

    for (const Formats::Bytes& content : contents)
        header.contentsSize += AlignedSize(content.size());

    for (const SaveEntry& entry : entries)
        header.filesSize += static_cast<std::uint32_t>(Formats::BackupFileHeaderSize) + AlignedSize(entry.data.size());

    header.totalSize = Formats::BackupArchiveTotalSize(header);

    Formats::Bytes archive = Formats::WriteBackupArchiveHeader(header);
    Formats::BackupFillNumbers fill(fillSeed);

    Append(archive, titleMetadata);
    FillToAlignment(archive, fill);

    for (const Formats::Bytes& content : contents)
    {
        Append(archive, content);
        FillToAlignment(archive, fill);
    }

    for (const SaveEntry& entry : entries)
        AppendFile(archive, entry, services, fill);

    Formats::Sha1Digest digest = Formats::ComputeSha1(archive);
    Ios::SignedData signedDigest = services.identity.Sign(digest);

    Formats::Bytes signatureSlot = signedDigest.signature;
    signatureSlot.resize(Formats::BackupSignatureSlotSize, 0);
    Append(archive, signatureSlot);
    Append(archive, services.identity.GetDeviceCertificate());
    Append(archive, signedDigest.certificate);
    return archive;
}

// The bit of a position inside its byte of the bitmap.
Formats::Byte BitInItsByte(std::size_t position)
{
    return static_cast<Formats::Byte>(1 << (position % Formats::BitsPerByte));
}

void SetBitmapBit(std::array<Formats::Byte, Formats::BackupContentBitmapSize>& bitmap, std::size_t position)
{
    std::size_t byteIndex = position / Formats::BitsPerByte;
    bitmap.at(byteIndex) |= BitInItsByte(position);
}

bool IsBitmapBitSet(const std::array<Formats::Byte, Formats::BackupContentBitmapSize>& bitmap, std::size_t position)
{
    std::size_t byteIndex = position / Formats::BitsPerByte;
    if (byteIndex >= bitmap.size())
        return false;

    return (bitmap[byteIndex] & BitInItsByte(position)) != 0;
}

[[noreturn]] void FailAsDamaged(std::string_view problem)
{
    throw DataError(DataResult::DamagedBackup, std::string(problem));
}

Formats::ByteView TakePart(Formats::ByteView bytes, std::size_t offset, std::size_t size, std::string_view partName)
{
    if (!Formats::IsRangeInside(offset, size, bytes.size()))
        FailAsDamaged(std::format("the archive ends before its {}", partName));

    return bytes.subspan(offset, size);
}

}

Formats::Bytes WriteSaveArchive(DataManagementServices& services, Ios::TitleId titleId, const NandAccess& titleFiles,
    const std::array<Formats::Byte, Formats::TransferIdSize>& transferId)
{
    std::vector<SaveEntry> entries;
    CollectEntries(titleFiles, Ios::DataFolderPath(titleId), "", entries);

    Formats::BackupArchiveHeader header;
    header.titleId = titleId.ToValue();
    header.transferId = transferId;
    return AssembleArchive(services, header, {}, {}, entries, titleId.lowPart);
}

Formats::Bytes WriteChannelArchive(DataManagementServices& services, Ios::TitleId titleId)
{
    Formats::Bytes titleMetadataBytes = Require(services.titles.ReadTitleMetadataBytes(titleId), "read the title metadata");
    Formats::TitleMetadata metadata = Require(services.titles.ReadTitleMetadata(titleId), "read the title metadata");
    std::vector<std::uint32_t> presentIds = Require(services.titles.ListPresentContentIds(titleId), "list the contents");

    Formats::BackupArchiveHeader header;
    std::vector<Formats::Bytes> contents;
    for (std::size_t position = 0; position < metadata.contents.size(); ++position)
    {
        const Formats::ContentRecord& record = metadata.contents[position];
        if (Ios::IsSharedContent(record))
            continue;

        bool isPresent = std::find(presentIds.begin(), presentIds.end(), record.id) != presentIds.end();
        if (!isPresent && Ios::IsOptionalContent(record))
            continue;

        if (!isPresent)
            throw DataError(DataResult::Failed, std::format("the content {:08x} of {} is missing", record.id, titleId.ToString()));

        SetBitmapBit(header.contentBitmap, position);
        contents.push_back(Require(services.titles.ExportContent(titleId, record.id), std::format("export the content {:08x}", record.id)));
    }

    return AssembleArchive(services, header, titleMetadataBytes, contents, {}, titleId.lowPart);
}

BackupArchive ReadBackupArchive(Formats::ByteView bytes)
{
    BackupArchive archive;
    try
    {
        archive.header = Formats::ReadBackupArchiveHeader(TakePart(bytes, 0, Formats::BackupArchiveHeaderSize, "header"));
        std::size_t position = Formats::BackupArchiveHeaderSize;

        archive.titleMetadataBytes = TakePart(bytes, position, archive.header.titleMetadataSize, "title metadata");
        position += AlignedSize(archive.header.titleMetadataSize);

        if (!archive.titleMetadataBytes.empty())
        {
            Formats::TitleMetadata metadata = Formats::ReadTitleMetadata(archive.titleMetadataBytes);
            for (std::size_t recordPosition = 0; recordPosition < metadata.contents.size(); ++recordPosition)
            {
                if (!IsBitmapBitSet(archive.header.contentBitmap, recordPosition))
                    continue;

                const Formats::ContentRecord& record = metadata.contents[recordPosition];
                std::size_t encryptedSize = Formats::RoundUpToMultiple(record.size, ContentEncryptionAlignment);
                archive.contents.push_back({record, TakePart(bytes, position, encryptedSize, "contents")});
                position += AlignedSize(record.size);
            }
        }

        for (std::uint32_t fileIndex = 0; fileIndex < archive.header.fileCount; ++fileIndex)
        {
            Formats::BackupFileHeader fileHeader = Formats::ReadBackupFileHeader(TakePart(bytes, position, Formats::BackupFileHeaderSize, "files"));
            position += Formats::BackupFileHeaderSize;

            std::size_t encryptedSize = Formats::RoundUpToMultiple(fileHeader.size, FileEncryptionAlignment);
            archive.files.push_back({fileHeader, TakePart(bytes, position, encryptedSize, "files")});
            position += AlignedSize(fileHeader.size);
        }
    }
    catch (const Formats::FormatError& error)
    {
        FailAsDamaged(error.what());
    }
    return archive;
}

void CheckArchiveSignature(const DataManagementServices& services, Formats::ByteView archiveBytes, const Formats::BackupArchiveHeader& header)
{
    std::uint32_t totalSize = header.totalSize;
    bool isSizeSensible = totalSize % SignedSizeAlignment == 0 && totalSize > Formats::BackupTrailerSize && totalSize <= archiveBytes.size();
    if (!isSizeSensible)
        FailAsDamaged(std::format("the archive says it has {:#x} bytes", totalSize));

    std::size_t signedSize = totalSize - Formats::BackupTrailerSize;
    Formats::Sha1Digest digest = Formats::ComputeSha1(archiveBytes.first(signedSize));

    Formats::ByteView trailer = archiveBytes.subspan(signedSize, Formats::BackupTrailerSize);
    Formats::ByteView signature = trailer.first(SignatureSize);
    Formats::ByteView certificates = trailer.subspan(Formats::BackupSignatureSlotSize);

    Formats::Bytes chain = services.authorityCertificates;
    chain.insert(chain.end(), certificates.begin(), certificates.end());

    if (services.identity.VerifySignature(digest, signature, chain).Failed())
        FailAsDamaged("the signature of the archive does not fit");
}

Formats::Bytes EncryptForCard(const Ios::ConsoleIdentityService& identity, Formats::ByteView plainBytes)
{
    Ios::Result<Ios::CipherResult> encrypted =
        identity.Encrypt(Ios::ConsoleIdentityService::SdCardKeyNumber, Formats::SdCardInitializationVector, plainBytes);
    return Require(encrypted, "encrypt for the SD card").data;
}

Formats::Bytes DecryptFromCard(const Ios::ConsoleIdentityService& identity, Formats::ByteView encryptedBytes)
{
    Ios::Result<Ios::CipherResult> decrypted =
        identity.Decrypt(Ios::ConsoleIdentityService::SdCardKeyNumber, Formats::SdCardInitializationVector, encryptedBytes);
    if (decrypted.Failed())
        FailAsDamaged("the header cannot be decrypted");

    return decrypted.Value().data;
}

void PrepareBackupFile(Ios::SdCardService& sdCard, const Ios::SdPath& file, std::size_t headerSize)
{
    std::optional<std::uint64_t> size = SdFileSize(sdCard, file);
    if (size && *size > headerSize)
        throw DataError(DataResult::AlreadyThere, std::format("{} is on the SD card already", file.ToString()));

    DeleteSdFileIfPresent(sdCard, file);
    MakeSdFolders(sdCard, file.Parent());
}

void BecomeSystemMenuAgain(Ios::TitleService& titles)
{
    titles.BecomeTitle(Ios::SystemMenuTitleId);
}

std::array<Formats::Byte, Formats::TransferIdSize> ReadTransferId(const NandAccess& menuFiles)
{
    std::array<Formats::Byte, Formats::TransferIdSize> transferId = {};

    Ios::NandPath path = MakeNandPath(TransferIdPath);
    if (!menuFiles.Exists(path))
        return transferId;

    Formats::Bytes file = menuFiles.ReadFile(path);
    std::size_t count = std::min(file.size(), transferId.size());
    std::copy_n(file.begin(), count, transferId.begin());
    return transferId;
}

}
