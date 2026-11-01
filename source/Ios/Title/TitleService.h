#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/Certificate.h"
#include "Formats/Sha1.h"
#include "Formats/Ticket.h"
#include "Formats/TitleMetadata.h"
#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/Identity/ConsoleIdentityService.h"
#include "Ios/Profile/ProfileChoice.h"
#include "Ios/Result.h"
#include "Ios/Title/ContentStream.h"
#include "Ios/Title/KernelFiles.h"
#include "Ios/Title/TicketView.h"
#include "Ios/Title/TitleId.h"
#include "Ios/Title/TitleMetadataView.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace Ios
{

// What has to be true before a title starts, found out by PrepareLaunch, and what the program needs to start it.
struct LaunchPlan
{
    TitleId titleId;

    // The IOS the title runs on. The service has made sure it is installed. Zero for an IOS itself.
    TitleId requiredSystemVersion;

    // The user and the group the title runs as. The file system holds the title to them.
    Caller caller;

    // What the title may do with the hardware, as the title metadata lists it.
    std::uint32_t accessRights;

    // The index of the content to start.
    std::uint16_t bootIndex;

    TicketView ticketView;
};

// The title service of IOS, ES in the original: it knows the titles on the NAND, their tickets and contents,
// and the program that runs. It takes the place of /dev/es. It follows the ES module of IOS80 (docs/audit/Ios.md).
//
// It reads and writes the NAND as the kernel does, through the file system service, which must outlive it.
// The identity of the console tells which tickets are for this console.
// Errors of the file system come back unchanged. The codes of ES are the ones of IosError.
// Calls from several threads are safe.
//
// The profile says which IOS this is. A call marked "Command" gives TitleInvalidArgument when that IOS does not have the command,
// as the original does for a number it does not know.
class TitleService
{
public:
    // The file system service and the identity must outlive this object. Without a profile this is the title service of IOS80.
    TitleService(FileSystemService& fileSystem, const ConsoleIdentityService& consoleIdentity, const IosProfile& iosProfile = SystemMenuProfile());

    // Every title that has a folder under /title, in the order of the file system. A title that is being installed is not there yet.
    Result<std::vector<TitleId>> ListInstalledTitles() const;

    // Every title that has a ticket file under /ticket. The IOS that read tickets of version 1 count their files (.tv1) too.
    Result<std::vector<TitleId>> ListTitlesWithTickets() const;

    // The ids of the contents of an installed title that are on the NAND, in the order of the title metadata.
    // A private content is on the NAND when its file is there, a shared content when /shared1/content.map names its hash.
    Result<std::vector<std::uint32_t>> ListPresentContentIds(TitleId titleId) const;

    // The same for any title metadata, also one of a title that is not installed. Command GetStoredContents.
    Result<std::vector<std::uint32_t>> ListPresentContentIds(const Formats::TitleMetadata& metadata) const;

    // The hashes of the contents that titles share. Command GetSharedContents.
    Result<std::vector<Formats::Sha1Digest>> ListSharedContentHashes() const;

    // The title metadata of an installed title, as it is stored with its signature. NotFound when the title is not installed.
    // Command GetStoredTitleMetadata. The parsed form is for the parts of the system that read it without a command, such as Boot.
    Result<Formats::Bytes> ReadTitleMetadataBytes(TitleId titleId) const;
    Result<Formats::TitleMetadata> ReadTitleMetadata(TitleId titleId) const;
    Result<TitleMetadataView> ReadTitleMetadataView(TitleId titleId) const;

    // True when the console has the key that only Korean consoles have. Command CheckHasKoreanKey.
    Result<bool> CheckHasKoreanKey() const;

    // The views of all the tickets of a title. A title without a ticket file has none.
    Result<std::vector<TicketView>> ListTicketViews(TitleId titleId) const;

    // Opens a content of an installed title for a program that has a ticket for it. The view must be one of the tickets of the title.
    // TicketInvalid when the view belongs to another title, TicketNotFound when the NAND has no such ticket,
    // TitleInvalidArgument when the title has no content with this index and ContentHashMismatch
    // when the content on the NAND is not the one the title metadata describes.
    Result<ContentStream> OpenContent(TitleId titleId, const TicketView& view, std::uint16_t contentIndex) const;

    // Opens a content of the title that runs now.
    Result<ContentStream> OpenOwnContent(std::uint16_t contentIndex) const;

    // The folder where a title keeps its own files. The title service does not look at whether it is there.
    NandPath GetDataDirectory(TitleId titleId) const;

    // The title that runs now, once a program has been entered.
    std::optional<TitleId> GetRunningTitleId() const;

    // Who the running program is for the file system. The kernel until a program has been entered.
    Caller GetCaller() const;

    // The System Menu looks after the data of other titles: from now on the caller is the user and the group of that title.
    // AccessRightsIncorrect when the running title is not the System Menu.
    Result<void> BecomeTitle(TitleId titleId);

    // A title without a data folder gets one, owned by the title, when it starts: ES makes the folder of a title that has none at its launch,
    // as DIVerify does for a disc (WiiBrew). The titles that NandTool imports from the dump come without their data folders.
    Result<void> EnsureDataFolder(TitleId titleId);

    // Checks that the program may start, as the console does before it reloads IOS for a title, and works out who it runs as.
    //  - TicketInvalid: the view belongs to another title. A view with the low half 0 belongs to every title of its family.
    //  - TicketNotFound: the NAND has no ticket with the id of the view.
    //  - NotFound: the title is not installed.
    //  - RequiredIosNotInstalled: the IOS the title metadata names is not installed.
    // A title that has no user id yet gets one.
    Result<LaunchPlan> PrepareLaunch(TitleId titleId, const TicketView& view);

    // The program of the plan runs from now on. Whoever starts it, the application host, does it after this call.
    void EnterTitle(const LaunchPlan& plan);

    // Installing. A title is installed in four steps: the ticket, the start (with the title metadata), the contents one after the other
    // and the end. The signature of the ticket and of the title metadata is checked against the certificates the caller gives
    // and the ones the NAND has kept in /sys/cert.sys. Only one title is installed at a time.

    // Puts the ticket with the other tickets of the title. A ticket with the same ticket id is replaced.
    //  - IssuerNotFound, SignatureInvalid: the certificates do not vouch for the signature.
    //  - DeviceIdMismatch: the ticket is for another console.
    //  - TicketInvalid: the ticket cannot be read.
    Result<void> ImportTicket(Formats::ByteView ticket, Formats::ByteView certificateChain);

    // Starts the installation of a title. An installation that was started before and is not finished is cancelled.
    //  - TitleInvalidArgument: the title metadata cannot be read.
    //  - IssuerNotFound, SignatureInvalid: the certificates do not vouch for the signature.
    //  - HigherVersionInstalled: a newer version of the title is installed.
    //  - TicketNotFound: the NAND has no ticket for the title, which holds the key of its contents.
    //  - TicketInvalid: the title key of the ticket cannot be decrypted. Tickets that belong to one console are not supported.
    Result<void> BeginInstallation(Formats::ByteView titleMetadata, Formats::ByteView certificateChain);

    // Decrypts a content as it is stored in a WAD file and keeps it. The hash is checked against the title metadata.
    // A shared content that is on the NAND already is not stored again.
    //  - TitleInvalidArgument: no installation was started, or the title metadata has no content with this id.
    //  - ContentHashMismatch: the content is not the one of the title metadata.
    Result<void> AddContent(std::uint32_t contentId, Formats::ByteView encryptedContent);

    // The same for a content that was decrypted before, as WeeOS's prepared Wii files keep it. The hash is checked the same way.
    Result<void> AddDecryptedContent(std::uint32_t contentId, Formats::ByteView content);

    // Makes the title available. The system titles must have all their contents that are not optional (TitleInvalidArgument).
    // Installing over an older version replaces the contents and keeps the data folder of the title.
    Result<void> FinishInstallation();

    // Throws away what was installed so far. The shared contents stay, as on the console.
    void CancelInstallation();

    // The four steps for a WAD file. Cancels the installation when a step fails.
    Result<void> InstallWad(Formats::ByteView wadFile);

    // Removing. The System Menu, boot2 and the IOS versions cannot be removed: TitleInvalidArgument.

    // Deletes the folder of the title with its data. The family folder goes too when it is empty then. The tickets stay.
    Result<void> DeleteTitle(TitleId titleId);

    // Deletes all the files of contents of a title and keeps its title metadata.
    Result<void> DeleteTitleContents(TitleId titleId);

    // Deletes one content. TitleInvalidArgument for a shared content and for an id that the title does not have.
    // Command DeleteTitleContent.
    Result<void> DeleteContent(TitleId titleId, std::uint32_t contentId);

    // Takes the ticket of the view out of the ticket file of its title. The file goes when it has no ticket left.
    Result<void> DeleteTicket(const TicketView& view);

    // Exporting. The content of an installed title as a WAD file keeps it: encrypted with the title key, padded to whole blocks.
    // The title metadata is ReadTitleMetadataBytes. Installing the result again gives the same content.
    Result<Formats::Bytes> ExportContent(TitleId titleId, std::uint32_t contentId) const;

private:
    // The title that is being installed: its title metadata and the key of its contents.
    struct Installation
    {
        Formats::TitleMetadata metadata;
        Formats::AesKey titleKey;
    };

    Result<void> RequireCommand(int command) const;
    Result<std::vector<std::uint32_t>> FindPresentContentIds(const Formats::TitleMetadata& metadata) const;
    Result<std::vector<Formats::Ticket>> ReadTickets(TitleId titleId) const;
    Result<Formats::Ticket> FindTicket(TitleId titleId, std::uint64_t ticketId) const;
    Result<Formats::AesKey> ReadTitleKey(TitleId titleId) const;
    Result<ContentStream> OpenContentRecord(const Formats::TitleMetadata& metadata, std::uint16_t contentIndex) const;
    Result<std::uint32_t> FindOrAssignUserId(TitleId titleId);

    Result<void> CheckSignature(Formats::ByteView signedFile, Formats::ByteView certificateChain) const;
    Result<void> PrepareImportFolder(TitleId titleId, Formats::ByteView titleMetadataBytes);
    void DiscardImport(TitleId titleId);
    Result<void> StoreTicket(Formats::ByteView ticketBytes, const Formats::Ticket& ticket);
    Result<void> StorePrivateContent(const Installation& currentInstallation, const Formats::ContentRecord& record, Formats::ByteView content);
    Result<void> StoreSharedContent(const Formats::ContentRecord& record, Formats::ByteView content);
    Result<void> CheckAllContentsAreThere(const Installation& currentInstallation) const;
    Result<void> PublishInstallation(const Installation& currentInstallation);
    Result<void> MoveImportIntoPlace(TitleId titleId);
    Result<void> MakeDataFolder(TitleId titleId, std::uint16_t groupId);
    Result<void> RemoveFolderIfEmpty(const NandPath& folder);
    void CancelInstallationWhileLocked();

    KernelFiles files;
    const ConsoleIdentityService& identity;
    const IosProfile profile;

    mutable std::mutex mutex;
    std::optional<TitleId> runningTitleId;
    Caller caller = SuperUserCaller;

    // Guards the installation, which takes many calls. A call that installs holds it until it ends. Take it before the other mutex.
    std::mutex installationMutex;
    std::optional<Installation> installation;
};

}
