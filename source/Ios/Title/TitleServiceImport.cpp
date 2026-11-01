#include "Ios/Title/TitleService.h"

#include "Formats/FormatError.h"
#include "Formats/Sha1.h"
#include "Formats/SignatureVerification.h"
#include "Formats/TitleEncryption.h"
#include "Formats/WadFile.h"
#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/SharedContentMap.h"
#include "Ios/Title/StoredFiles.h"
#include "Ios/Title/TitlePaths.h"
#include "Ios/Title/UserIdTable.h"

#include <algorithm>
#include <mutex>

namespace Ios
{

namespace
{

// A ticket with the console id 0 may be used on every console.
constexpr std::uint32_t EveryConsoleId = 0;

// The content id in a file name such as "00000011.app", or nothing for another name such as "title.tmd".
std::optional<std::uint32_t> ReadContentIdFromFileName(std::string_view name)
{
    if (!name.ends_with(ContentFileExtension))
        return std::nullopt;

    name.remove_suffix(ContentFileExtension.size());
    return ParseNameNumber(name);
}

// The certificates that the NAND has kept in /sys/cert.sys. A NAND without the file, or with a damaged one, has none.
std::vector<Formats::Certificate> ReadKeptCertificates(const KernelFiles& files)
{
    Result<Formats::Bytes> bytes = files.ReadFile(CertificateFilePath());
    if (bytes.Failed())
        return {};

    try
    {
        return Formats::ReadCertificateChain(bytes.Value());
    }
    catch (const Formats::FormatError&)
    {
        return {};
    }
}

}

Result<void> TitleService::CheckSignature(Formats::ByteView signedFile, Formats::ByteView certificateChain) const
{
    std::vector<Formats::Certificate> certificates;
    try
    {
        certificates = Formats::ReadCertificateChain(certificateChain);
    }
    catch (const Formats::FormatError&)
    {
        return IosError::TitleInvalidArgument;
    }

    std::vector<Formats::Certificate> keptCertificates = ReadKeptCertificates(files);
    certificates.insert(certificates.end(), keptCertificates.begin(), keptCertificates.end());

    switch (Formats::VerifySignedFile(signedFile, certificates))
    {
    case Formats::SignatureCheck::Valid:
        return Result<void>();

    case Formats::SignatureCheck::SignerNotFound:
        return IosError::IssuerNotFound;

    case Formats::SignatureCheck::Invalid:
        return IosError::SignatureInvalid;
    }

    return IosError::SignatureInvalid;
}

Result<void> TitleService::ImportTicket(Formats::ByteView ticketBytes, Formats::ByteView certificateChain)
{
    Result<Formats::Ticket> ticket = ParseTicket(ticketBytes, IosError::TicketInvalid);
    if (ticket.Failed())
        return ticket.Error();

    bool isForEveryConsole = ticket.Value().consoleId == EveryConsoleId;
    bool isForThisConsole = ticket.Value().consoleId == identity.GetDeviceId();
    if (!isForEveryConsole && !isForThisConsole)
        return IosError::DeviceIdMismatch;

    Result<void> signatureChecked = CheckSignature(ticketBytes, certificateChain);
    if (signatureChecked.Failed())
        return signatureChecked;

    return StoreTicket(ticketBytes.first(StoredTicketSize), ticket.Value());
}

Result<void> TitleService::StoreTicket(Formats::ByteView ticketBytes, const Formats::Ticket& ticket)
{
    TitleId titleId = TitleId::FromValue(ticket.titleId);

    Result<void> folderMade = files.EnsureDirectory(TicketFamilyPath(titleId.highPart), SystemEntryPermissions);
    if (folderMade.Failed())
        return folderMade;

    std::vector<Formats::Bytes> storedTickets;
    Result<Formats::Bytes> storedFile = files.ReadFile(TicketFilePath(titleId));
    if (storedFile.Succeeded())
        storedTickets = SplitStoredTickets(storedFile.Value());
    else if (storedFile.Error() != IosError::NotFound)
        return storedFile.Error();

    Formats::Bytes newTicket(ticketBytes.begin(), ticketBytes.end());
    auto sameTicket = std::find_if(storedTickets.begin(), storedTickets.end(),
        [&ticket](const Formats::Bytes& stored) { return HasTicketId(stored, ticket.ticketId); });

    if (sameTicket != storedTickets.end())
        *sameTicket = newTicket;
    else
        storedTickets.push_back(newTicket);

    return files.WriteFile(TicketFilePath(titleId), JoinStoredTickets(storedTickets));
}

Result<void> TitleService::BeginInstallation(Formats::ByteView titleMetadataBytes, Formats::ByteView certificateChain)
{
    std::lock_guard<std::mutex> lock(installationMutex);
    CancelInstallationWhileLocked();

    Result<Formats::TitleMetadata> metadata = ParseTitleMetadata(titleMetadataBytes, IosError::TitleInvalidArgument);
    if (metadata.Failed())
        return metadata.Error();

    Result<void> signatureChecked = CheckSignature(titleMetadataBytes, certificateChain);
    if (signatureChecked.Failed())
        return signatureChecked;

    TitleId titleId = TitleId::FromValue(metadata.Value().titleId);
    Result<Formats::TitleMetadata> installedMetadata = ReadTitleMetadata(titleId);
    bool isOlderThanInstalled = installedMetadata.Succeeded() && installedMetadata.Value().titleVersion > metadata.Value().titleVersion;
    if (isOlderThanInstalled)
        return IosError::HigherVersionInstalled;

    Result<Formats::AesKey> titleKey = ReadTitleKey(titleId);
    if (titleKey.Failed())
        return titleKey.Error();

    Result<void> prepared = PrepareImportFolder(titleId, titleMetadataBytes);
    if (prepared.Failed())
    {
        DiscardImport(titleId);
        return prepared;
    }

    installation = Installation{metadata.Value(), titleKey.Value()};
    return Result<void>();
}

Result<void> TitleService::PrepareImportFolder(TitleId titleId, Formats::ByteView titleMetadataBytes)
{
    Result<void> leftoversRemoved = files.DeleteIfPresent(ImportTitlePath(titleId));
    if (leftoversRemoved.Failed())
        return leftoversRemoved;

    Result<void> familyMade = files.EnsureDirectory(ImportFamilyPath(titleId.highPart), SystemEntryPermissions);
    if (familyMade.Failed())
        return familyMade;

    Result<void> titleMade = files.EnsureDirectory(ImportTitlePath(titleId), TitleFolderPermissions);
    if (titleMade.Failed())
        return titleMade;

    Result<void> contentsMade = files.EnsureDirectory(ImportContentFolderPath(titleId), SystemEntryPermissions);
    if (contentsMade.Failed())
        return contentsMade;

    return files.WriteFile(TemporaryMetadataPath(), titleMetadataBytes);
}

Result<void> TitleService::AddContent(std::uint32_t contentId, Formats::ByteView encryptedContent)
{
    std::lock_guard<std::mutex> lock(installationMutex);
    if (!installation)
        return IosError::TitleInvalidArgument;

    const Formats::ContentRecord* record = FindContentById(installation->metadata, contentId);
    if (record == nullptr)
        return IosError::TitleInvalidArgument;

    Formats::Bytes content;
    try
    {
        content = Formats::DecryptContent(encryptedContent, installation->titleKey, *record);
    }
    catch (const Formats::FormatError&)
    {
        return IosError::ContentHashMismatch;
    }

    if (IsSharedContent(*record))
        return StoreSharedContent(*record, content);

    return StorePrivateContent(*installation, *record, content);
}

Result<void> TitleService::AddDecryptedContent(std::uint32_t contentId, Formats::ByteView content)
{
    std::lock_guard<std::mutex> lock(installationMutex);
    if (!installation)
        return IosError::TitleInvalidArgument;

    const Formats::ContentRecord* record = FindContentById(installation->metadata, contentId);
    if (record == nullptr)
        return IosError::TitleInvalidArgument;

    bool hasTheRightContent = content.size() == record->size && Formats::ComputeSha1(content) == record->hash;
    if (!hasTheRightContent)
        return IosError::ContentHashMismatch;

    if (IsSharedContent(*record))
        return StoreSharedContent(*record, content);

    return StorePrivateContent(*installation, *record, content);
}

Result<void> TitleService::StorePrivateContent(const Installation& currentInstallation, const Formats::ContentRecord& record,
    Formats::ByteView content)
{
    TitleId titleId = TitleId::FromValue(currentInstallation.metadata.titleId);
    NandPath waitingPath = TemporaryContentPath(record.id);

    Result<void> written = files.WriteFile(waitingPath, content);
    if (written.Failed())
        return written;

    Result<void> moved = files.Rename(waitingPath, ImportContentPath(titleId, record.id));
    if (moved.Failed())
        files.DeleteIfPresent(waitingPath);

    return moved;
}

Result<void> TitleService::StoreSharedContent(const Formats::ContentRecord& record, Formats::ByteView content)
{
    Result<SharedContentMap> map = SharedContentMap::Read(files);
    if (map.Failed())
        return map.Error();

    bool isOnTheNandAlready = map.Value().FindName(record.hash).has_value();
    if (isOnTheNandAlready)
        return Result<void>();

    Result<void> folderMade = files.EnsureDirectory(SharedContentFolderPath(), SystemEntryPermissions);
    if (folderMade.Failed())
        return folderMade;

    std::string name = map.Value().MakeNextName();
    NandPath waitingPath = TemporarySharedContentPath(name);

    Result<void> written = files.WriteFile(waitingPath, content);
    if (written.Failed())
        return written;

    Result<void> moved = files.Rename(waitingPath, SharedContentPath(name));
    if (moved.Failed())
    {
        files.DeleteIfPresent(waitingPath);
        return moved;
    }

    map.Value().Add(SharedContentEntry{name, record.hash});
    Result<void> mapWritten = map.Value().Write(files);
    if (mapWritten.Failed())
        files.DeleteIfPresent(SharedContentPath(name));

    return mapWritten;
}

Result<void> TitleService::CheckAllContentsAreThere(const Installation& currentInstallation) const
{
    TitleId titleId = TitleId::FromValue(currentInstallation.metadata.titleId);
    if (!IsSystemSoftware(titleId))
        return Result<void>();

    Result<SharedContentMap> map = SharedContentMap::Read(files);
    if (map.Failed())
        return map.Error();

    for (const Formats::ContentRecord& record : currentInstallation.metadata.contents)
    {
        if (IsOptionalContent(record))
            continue;

        bool isThere = false;
        if (IsSharedContent(record))
            isThere = map.Value().FindName(record.hash).has_value();
        else
            isThere = files.Exists(ImportContentPath(titleId, record.id));

        if (!isThere)
            return IosError::TitleInvalidArgument;
    }

    return Result<void>();
}

Result<void> TitleService::FinishInstallation()
{
    std::lock_guard<std::mutex> lock(installationMutex);
    if (!installation)
        return IosError::TitleInvalidArgument;

    Result<void> complete = CheckAllContentsAreThere(*installation);
    if (complete.Failed())
        return complete;

    Result<void> published = PublishInstallation(*installation);
    if (published.Failed())
        return published;

    installation.reset();
    return Result<void>();
}

Result<void> TitleService::PublishInstallation(const Installation& currentInstallation)
{
    TitleId titleId = TitleId::FromValue(currentInstallation.metadata.titleId);

    Result<void> metadataMoved = files.Rename(TemporaryMetadataPath(), ImportMetadataPath(titleId));
    if (metadataMoved.Failed())
        return metadataMoved;

    Result<void> moved = MoveImportIntoPlace(titleId);
    if (moved.Failed())
        return moved;

    Result<void> dataFolderMade = MakeDataFolder(titleId, currentInstallation.metadata.groupId);
    if (dataFolderMade.Failed())
        return dataFolderMade;

    return RemoveFolderIfEmpty(ImportFamilyPath(titleId.highPart));
}

// An older version of the title keeps its data folder. Only the folder of its contents is replaced.
Result<void> TitleService::MoveImportIntoPlace(TitleId titleId)
{
    if (files.Exists(TitlePath(titleId)))
    {
        Result<void> contentsMoved = files.Rename(ImportContentFolderPath(titleId), ContentFolderPath(titleId));
        if (contentsMoved.Failed())
            return contentsMoved;

        return files.DeleteIfPresent(ImportTitlePath(titleId));
    }

    Result<void> familyFolderMade = files.EnsureDirectory(TitleFamilyPath(titleId.highPart), TitleFolderPermissions);
    if (familyFolderMade.Failed())
        return familyFolderMade;

    return files.Rename(ImportTitlePath(titleId), TitlePath(titleId));
}

// ES does this in its start (0x20105538 in IOS80, 0x20104F20 in boot2). The original stops at the first error;
// here a title that fails is left and the next one is settled.
void TitleService::SettleInterruptedInstallations()
{
    std::lock_guard<std::mutex> lock(installationMutex);

    Result<std::vector<std::string>> familyNames = files.ListNames(ImportsPath());
    if (familyNames.Failed())
        return;

    for (const std::string& familyName : familyNames.Value())
    {
        std::optional<std::uint32_t> highPart = ParseNameNumber(familyName);
        if (highPart)
            SettleInterruptedFamily(*highPart);
    }
}

void TitleService::SettleInterruptedFamily(std::uint32_t highPart)
{
    Result<std::vector<std::string>> titleNames = files.ListNames(ImportFamilyPath(highPart));
    if (titleNames.Failed())
        return;

    for (const std::string& titleName : titleNames.Value())
    {
        std::optional<std::uint32_t> lowPart = ParseNameNumber(titleName);
        if (lowPart)
            SettleInterruptedInstallation(TitleId{highPart, *lowPart});
    }

    RemoveFolderIfEmpty(ImportFamilyPath(highPart));
}

Result<void> TitleService::SettleInterruptedInstallation(TitleId titleId)
{
    Result<Formats::Bytes> metadataBytes = files.ReadFile(ImportMetadataPath(titleId));
    if (metadataBytes.Failed() && metadataBytes.Error() != IosError::NotFound)
        return metadataBytes.Error();

    if (metadataBytes.Failed())
        return files.Delete(ImportTitlePath(titleId));

    // The original does not read the metadata beyond its contents. Metadata that cannot be read counts as missing here.
    Result<Formats::TitleMetadata> metadata = ParseTitleMetadata(metadataBytes.Value(), IosError::TitleInvalidArgument);
    if (metadata.Failed())
        return files.Delete(ImportTitlePath(titleId));

    Result<void> leftoversDeleted = DeleteContentsNotInMetadata(titleId, metadata.Value());
    if (leftoversDeleted.Failed())
        return leftoversDeleted;

    return MoveImportIntoPlace(titleId);
}

Result<void> TitleService::DeleteContentsNotInMetadata(TitleId titleId, const Formats::TitleMetadata& metadata)
{
    NandPath contentFolder = ImportContentFolderPath(titleId);
    Result<std::vector<std::string>> names = files.ListNames(contentFolder);
    if (names.Failed())
        return names.Error();

    for (const std::string& name : names.Value())
    {
        std::optional<std::uint32_t> contentId = ReadContentIdFromFileName(name);
        if (!contentId)
            continue;

        bool isListed = FindContentById(metadata, *contentId) != nullptr;
        if (isListed)
            continue;

        Result<void> deleted = files.Delete(contentFolder.Child(name).Value());
        if (deleted.Failed())
            return deleted;
    }

    return Result<void>();
}

Result<void> TitleService::EnsureDataFolder(TitleId titleId)
{
    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    return MakeDataFolder(titleId, metadata.Value().groupId);
}

// A title without a data folder gets one that belongs to its own user.
Result<void> TitleService::MakeDataFolder(TitleId titleId, std::uint16_t groupId)
{
    if (files.Exists(DataFolderPath(titleId)))
        return Result<void>();

    Result<std::uint32_t> userId = FindOrAssignUserId(titleId);
    if (userId.Failed())
        return userId.Error();

    Result<void> folderMade = files.EnsureDirectory(DataFolderPath(titleId), OwnerOnlyPermissions);
    if (folderMade.Failed())
        return folderMade;

    return files.SetOwner(DataFolderPath(titleId), userId.Value(), groupId);
}

void TitleService::CancelInstallation()
{
    std::lock_guard<std::mutex> lock(installationMutex);
    CancelInstallationWhileLocked();
}

void TitleService::CancelInstallationWhileLocked()
{
    if (!installation)
        return;

    DiscardImport(TitleId::FromValue(installation->metadata.titleId));
    installation.reset();
}

void TitleService::DiscardImport(TitleId titleId)
{
    files.DeleteIfPresent(TemporaryMetadataPath());
    files.DeleteIfPresent(ImportTitlePath(titleId));
    RemoveFolderIfEmpty(ImportFamilyPath(titleId.highPart));
}

Result<void> TitleService::InstallWad(Formats::ByteView wadFile)
{
    Formats::WadSections sections;
    Formats::TitleMetadata metadata;
    try
    {
        sections = Formats::ReadWadSections(wadFile);
        metadata = Formats::ReadTitleMetadata(sections.titleMetadata);
    }
    catch (const Formats::FormatError&)
    {
        return IosError::TitleInvalidArgument;
    }

    Result<void> ticketImported = ImportTicket(sections.ticket, sections.certificateChain);
    if (ticketImported.Failed())
        return ticketImported;

    Result<void> started = BeginInstallation(sections.titleMetadata, sections.certificateChain);
    if (started.Failed())
        return started;

    for (std::size_t index = 0; index < metadata.contents.size(); ++index)
    {
        Result<void> added = AddContent(metadata.contents[index].id, sections.encryptedContents[index]);
        if (added.Failed())
        {
            CancelInstallation();
            return added;
        }
    }

    Result<void> finished = FinishInstallation();
    if (finished.Failed())
        CancelInstallation();

    return finished;
}

}
