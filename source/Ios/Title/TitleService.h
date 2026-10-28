#pragma once

#include "Formats/Bytes.h"
#include "Formats/Sha1.h"
#include "Formats/Ticket.h"
#include "Formats/TitleMetadata.h"
#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/FileSystem/NandPath.h"
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
// Errors of the file system come back unchanged. The codes of ES are the ones of IosError.
// Calls from several threads are safe.
class TitleService
{
public:
    explicit TitleService(FileSystemService& fileSystem);

    // Every title that has a folder under /title, in the order of the file system. A title that is being installed is not there yet.
    Result<std::vector<TitleId>> ListInstalledTitles() const;

    // Every title that has a ticket file under /ticket.
    Result<std::vector<TitleId>> ListTitlesWithTickets() const;

    // The ids of the contents of an installed title that are on the NAND, in the order of the title metadata.
    // A private content is on the NAND when its file is there, a shared content when /shared1/content.map names its hash.
    Result<std::vector<std::uint32_t>> ListPresentContentIds(TitleId titleId) const;

    // The same for any title metadata, also one of a title that is not installed.
    Result<std::vector<std::uint32_t>> ListPresentContentIds(const Formats::TitleMetadata& metadata) const;

    // The hashes of the contents that titles share.
    Result<std::vector<Formats::Sha1Digest>> ListSharedContentHashes() const;

    // The title metadata of an installed title, as it is stored with its signature. NotFound when the title is not installed.
    Result<Formats::Bytes> ReadTitleMetadataBytes(TitleId titleId) const;
    Result<Formats::TitleMetadata> ReadTitleMetadata(TitleId titleId) const;
    Result<TitleMetadataView> ReadTitleMetadataView(TitleId titleId) const;

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

    // Checks that the program may start, as the console does before it reloads IOS for a title, and works out who it runs as.
    //  - TicketInvalid: the view belongs to another title. A view with the low half 0 belongs to every title of its family.
    //  - TicketNotFound: the NAND has no ticket with the id of the view.
    //  - NotFound: the title is not installed.
    //  - RequiredIosNotInstalled: the IOS the title metadata names is not installed.
    // A title that has no user id yet gets one.
    Result<LaunchPlan> PrepareLaunch(TitleId titleId, const TicketView& view);

    // The program of the plan runs from now on. Whoever starts it, the application host, does it after this call.
    void EnterTitle(const LaunchPlan& plan);

private:
    Result<std::vector<Formats::Ticket>> ReadTickets(TitleId titleId) const;
    Result<Formats::Ticket> FindTicket(TitleId titleId, std::uint64_t ticketId) const;
    Result<ContentStream> OpenContentRecord(const Formats::TitleMetadata& metadata, std::uint16_t contentIndex) const;
    Result<std::uint32_t> FindOrAssignUserId(TitleId titleId);

    KernelFiles files;

    mutable std::mutex mutex;
    std::optional<TitleId> runningTitleId;
    Caller caller = SuperUserCaller;
};

}
