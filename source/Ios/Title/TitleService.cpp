#include "Ios/Title/TitleService.h"

#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/SharedContentMap.h"
#include "Ios/Title/StoredFiles.h"
#include "Ios/Title/TitlePaths.h"
#include "Ios/Title/UserIdTable.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace Ios
{

namespace
{

constexpr std::string_view TicketFileExtension = ".tik";

// The number in the name of a ticket file such as "48414341.tik".
std::optional<std::uint32_t> ParseTicketFileName(std::string_view name)
{
    if (!name.ends_with(TicketFileExtension))
        return std::nullopt;

    name.remove_suffix(TicketFileExtension.size());
    return ParseNameNumber(name);
}

// The view of a title with the low half 0 stands for every title of the family.
bool TicketViewIsForTitle(const TicketView& view, TitleId titleId)
{
    if (view.titleId == titleId)
        return true;

    return view.titleId.highPart == titleId.highPart && view.titleId.lowPart == 0;
}

const Formats::ContentRecord* FindContentByIndex(const Formats::TitleMetadata& metadata, std::uint16_t contentIndex)
{
    auto found = std::find_if(metadata.contents.begin(), metadata.contents.end(),
        [contentIndex](const Formats::ContentRecord& record) { return record.index == contentIndex; });

    if (found == metadata.contents.end())
        return nullptr;

    return &*found;
}

}

TitleService::TitleService(FileSystemService& fileSystem, const ConsoleIdentityService& consoleIdentity)
    : files(fileSystem)
    , identity(consoleIdentity)
{
}

Result<std::vector<TitleId>> TitleService::ListInstalledTitles() const
{
    Result<std::vector<std::string>> familyNames = files.ListNames(TitlesPath());
    if (familyNames.Failed())
        return familyNames.Error();

    std::vector<TitleId> titleIds;
    for (const std::string& familyName : familyNames.Value())
    {
        std::optional<std::uint32_t> highPart = ParseNameNumber(familyName);
        if (!highPart)
            continue;

        Result<std::vector<std::string>> titleNames = files.ListNames(TitleFamilyPath(*highPart));
        if (titleNames.Failed())
            return titleNames.Error();

        for (const std::string& titleName : titleNames.Value())
        {
            std::optional<std::uint32_t> lowPart = ParseNameNumber(titleName);
            if (lowPart)
                titleIds.push_back({*highPart, *lowPart});
        }
    }

    return titleIds;
}

Result<std::vector<TitleId>> TitleService::ListTitlesWithTickets() const
{
    Result<std::vector<std::string>> familyNames = files.ListNames(TicketsPath());
    if (familyNames.Failed())
        return familyNames.Error();

    std::vector<TitleId> titleIds;
    for (const std::string& familyName : familyNames.Value())
    {
        std::optional<std::uint32_t> highPart = ParseNameNumber(familyName);
        if (!highPart)
            continue;

        Result<std::vector<std::string>> fileNames = files.ListNames(TicketFamilyPath(*highPart));
        if (fileNames.Failed())
            return fileNames.Error();

        for (const std::string& fileName : fileNames.Value())
        {
            std::optional<std::uint32_t> lowPart = ParseTicketFileName(fileName);
            if (lowPart)
                titleIds.push_back({*highPart, *lowPart});
        }
    }

    return titleIds;
}

Result<std::vector<std::uint32_t>> TitleService::ListPresentContentIds(TitleId titleId) const
{
    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    return ListPresentContentIds(metadata.Value());
}

Result<std::vector<std::uint32_t>> TitleService::ListPresentContentIds(const Formats::TitleMetadata& metadata) const
{
    Result<SharedContentMap> sharedContents = SharedContentMap::Read(files);
    if (sharedContents.Failed())
        return sharedContents.Error();

    TitleId titleId = TitleId::FromValue(metadata.titleId);

    std::vector<std::uint32_t> contentIds;
    for (const Formats::ContentRecord& record : metadata.contents)
    {
        bool isPresent = IsSharedContent(record) ? sharedContents.Value().FindName(record.hash).has_value()
                                                 : files.Exists(PrivateContentPath(titleId, record.id));
        if (isPresent)
            contentIds.push_back(record.id);
    }

    return contentIds;
}

Result<std::vector<Formats::Sha1Digest>> TitleService::ListSharedContentHashes() const
{
    Result<SharedContentMap> sharedContents = SharedContentMap::Read(files);
    if (sharedContents.Failed())
        return sharedContents.Error();

    std::vector<Formats::Sha1Digest> hashes;
    for (const SharedContentEntry& entry : sharedContents.Value().Entries())
        hashes.push_back(entry.hash);

    return hashes;
}

Result<Formats::Bytes> TitleService::ReadTitleMetadataBytes(TitleId titleId) const
{
    return files.ReadFile(TitleMetadataPath(titleId));
}

Result<Formats::TitleMetadata> TitleService::ReadTitleMetadata(TitleId titleId) const
{
    Result<Formats::Bytes> bytes = ReadTitleMetadataBytes(titleId);
    if (bytes.Failed())
        return bytes.Error();

    return ParseTitleMetadata(bytes.Value(), IosError::ShortRead);
}

Result<TitleMetadataView> TitleService::ReadTitleMetadataView(TitleId titleId) const
{
    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    return MakeTitleMetadataView(metadata.Value());
}

Result<std::vector<Formats::Ticket>> TitleService::ReadTickets(TitleId titleId) const
{
    std::vector<Formats::Ticket> tickets;

    Result<Formats::Bytes> bytes = files.ReadFile(TicketFilePath(titleId));
    if (bytes.Failed() && bytes.Error() == IosError::NotFound)
        return tickets;

    if (bytes.Failed())
        return bytes.Error();

    std::size_t ticketCount = bytes.Value().size() / StoredTicketSize;
    for (std::size_t ticketNumber = 0; ticketNumber < ticketCount; ++ticketNumber)
    {
        Formats::ByteView ticketBytes(bytes.Value().data() + ticketNumber * StoredTicketSize, StoredTicketSize);
        Result<Formats::Ticket> ticket = ParseTicket(ticketBytes, IosError::TicketInvalid);
        if (ticket.Failed())
            return ticket.Error();

        tickets.push_back(ticket.Value());
    }

    return tickets;
}

Result<Formats::Ticket> TitleService::FindTicket(TitleId titleId, std::uint64_t ticketId) const
{
    Result<std::vector<Formats::Ticket>> tickets = ReadTickets(titleId);
    if (tickets.Failed())
        return tickets.Error();

    for (const Formats::Ticket& ticket : tickets.Value())
    {
        if (ticket.ticketId == ticketId)
            return ticket;
    }

    return IosError::TicketNotFound;
}

Result<std::vector<TicketView>> TitleService::ListTicketViews(TitleId titleId) const
{
    Result<std::vector<Formats::Ticket>> tickets = ReadTickets(titleId);
    if (tickets.Failed())
        return tickets.Error();

    std::vector<TicketView> views;
    for (const Formats::Ticket& ticket : tickets.Value())
        views.push_back(MakeTicketView(ticket));

    return views;
}

Result<ContentStream> TitleService::OpenContent(TitleId titleId, const TicketView& view, std::uint16_t contentIndex) const
{
    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    if (!TicketViewIsForTitle(view, titleId))
        return IosError::TicketInvalid;

    Result<Formats::Ticket> ticket = FindTicket(view.titleId, view.ticketId);
    if (ticket.Failed())
        return ticket.Error();

    return OpenContentRecord(metadata.Value(), contentIndex);
}

Result<ContentStream> TitleService::OpenOwnContent(std::uint16_t contentIndex) const
{
    std::optional<TitleId> titleId = GetRunningTitleId();
    if (!titleId)
        return IosError::TitleInvalidArgument;

    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(*titleId);
    if (metadata.Failed())
        return metadata.Error();

    return OpenContentRecord(metadata.Value(), contentIndex);
}

Result<ContentStream> TitleService::OpenContentRecord(const Formats::TitleMetadata& metadata, std::uint16_t contentIndex) const
{
    const Formats::ContentRecord* record = FindContentByIndex(metadata, contentIndex);
    if (record == nullptr)
        return IosError::TitleInvalidArgument;

    TitleId titleId = TitleId::FromValue(metadata.titleId);
    NandPath contentPath = PrivateContentPath(titleId, record->id);
    if (IsSharedContent(*record))
    {
        Result<SharedContentMap> sharedContents = SharedContentMap::Read(files);
        if (sharedContents.Failed())
            return sharedContents.Error();

        std::optional<std::string> name = sharedContents.Value().FindName(record->hash);
        if (!name)
            return IosError::NotFound;

        contentPath = SharedContentPath(*name);
    }

    Result<Formats::Bytes> contentBytes = files.ReadFile(contentPath);
    if (contentBytes.Failed())
        return contentBytes.Error();

    bool isWhatTheMetadataDescribes = contentBytes.Value().size() == record->size && Formats::ComputeSha1(contentBytes.Value()) == record->hash;
    if (!isWhatTheMetadataDescribes)
        return IosError::ContentHashMismatch;

    return ContentStream(std::move(contentBytes.Value()));
}

NandPath TitleService::GetDataDirectory(TitleId titleId) const
{
    return DataFolderPath(titleId);
}

std::optional<TitleId> TitleService::GetRunningTitleId() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return runningTitleId;
}

Caller TitleService::GetCaller() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return caller;
}

Result<void> TitleService::BecomeTitle(TitleId titleId)
{
    if (GetRunningTitleId() != SystemMenuTitleId)
        return IosError::AccessRightsIncorrect;

    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    Result<std::uint32_t> userId = FindOrAssignUserId(titleId);
    if (userId.Failed())
        return userId.Error();

    std::lock_guard<std::mutex> lock(mutex);
    caller = Caller{userId.Value(), metadata.Value().groupId};
    return Result<void>();
}

Result<LaunchPlan> TitleService::PrepareLaunch(TitleId titleId, const TicketView& view)
{
    Result<Formats::Ticket> ticket = FindTicket(view.titleId, view.ticketId);
    if (ticket.Failed())
        return ticket.Error();

    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    if (!TicketViewIsForTitle(view, titleId))
        return IosError::TicketInvalid;

    TitleId requiredSystemVersion = TitleId::FromValue(metadata.Value().requiredSystemVersion);
    bool needsAnIos = requiredSystemVersion.ToValue() != 0;
    if (needsAnIos && !files.Exists(TitleMetadataPath(requiredSystemVersion)))
        return IosError::RequiredIosNotInstalled;

    Result<std::uint32_t> userId = FindOrAssignUserId(titleId);
    if (userId.Failed())
        return userId.Error();

    LaunchPlan plan;
    plan.titleId = titleId;
    plan.requiredSystemVersion = requiredSystemVersion;
    plan.caller = Caller{userId.Value(), metadata.Value().groupId};
    plan.accessRights = metadata.Value().accessRights;
    plan.bootIndex = metadata.Value().bootIndex;
    plan.ticketView = view;
    return plan;
}

void TitleService::EnterTitle(const LaunchPlan& plan)
{
    std::lock_guard<std::mutex> lock(mutex);
    runningTitleId = plan.titleId;
    caller = plan.caller;
}

Result<std::uint32_t> TitleService::FindOrAssignUserId(TitleId titleId)
{
    std::lock_guard<std::mutex> lock(mutex);

    Result<UserIdTable> table = UserIdTable::Read(files);
    if (table.Failed())
        return table.Error();

    std::uint32_t userId = table.Value().FindOrAdd(titleId);
    if (table.Value().HasChanged())
    {
        Result<void> written = table.Value().Write(files);
        if (written.Failed())
            return written.Error();
    }

    return userId;
}

}
