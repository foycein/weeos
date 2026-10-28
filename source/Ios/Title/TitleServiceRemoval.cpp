#include "Ios/Title/TitleService.h"

#include "Ios/Title/ContentTypes.h"
#include "Ios/Title/StoredFiles.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>
#include <string_view>

namespace Ios
{

namespace
{

constexpr std::string_view ContentFileExtension = ".app";

}

Result<void> TitleService::RemoveFolderIfEmpty(const NandPath& folder)
{
    Result<std::vector<std::string>> names = files.ListNames(folder);
    if (names.Failed())
        return names.Error();

    if (!names.Value().empty())
        return Result<void>();

    return files.Delete(folder);
}

Result<void> TitleService::DeleteTitle(TitleId titleId)
{
    if (IsSystemSoftware(titleId))
        return IosError::TitleInvalidArgument;

    Result<void> deleted = files.Delete(TitlePath(titleId));
    if (deleted.Failed())
        return deleted;

    return RemoveFolderIfEmpty(TitleFamilyPath(titleId.highPart));
}

Result<void> TitleService::DeleteTitleContents(TitleId titleId)
{
    if (IsSystemSoftware(titleId))
        return IosError::TitleInvalidArgument;

    NandPath contentFolder = ContentFolderPath(titleId);
    Result<std::vector<std::string>> names = files.ListNames(contentFolder);
    if (names.Failed())
        return names.Error();

    for (const std::string& name : names.Value())
    {
        if (!std::string_view(name).ends_with(ContentFileExtension))
            continue;

        Result<NandPath> contentPath = contentFolder.Child(name);
        if (contentPath.Failed())
            return contentPath.Error();

        Result<void> deleted = files.Delete(contentPath.Value());
        if (deleted.Failed())
            return deleted;
    }

    return Result<void>();
}

Result<void> TitleService::DeleteContent(TitleId titleId, std::uint32_t contentId)
{
    if (IsSystemSoftware(titleId))
        return IosError::TitleInvalidArgument;

    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    auto record = std::find_if(metadata.Value().contents.begin(), metadata.Value().contents.end(),
        [contentId](const Formats::ContentRecord& candidate) { return candidate.id == contentId; });

    bool canBeDeleted = record != metadata.Value().contents.end() && !IsSharedContent(*record);
    if (!canBeDeleted)
        return IosError::TitleInvalidArgument;

    return files.Delete(PrivateContentPath(titleId, contentId));
}

Result<void> TitleService::DeleteTicket(const TicketView& view)
{
    if (IsSystemSoftware(view.titleId))
        return IosError::TitleInvalidArgument;

    NandPath ticketFile = TicketFilePath(view.titleId);
    Result<Formats::Bytes> fileBytes = files.ReadFile(ticketFile);
    if (fileBytes.Failed())
        return fileBytes.Error();

    std::vector<Formats::Bytes> remainingTickets;
    for (Formats::Bytes& storedTicket : SplitStoredTickets(fileBytes.Value()))
    {
        if (!HasTicketId(storedTicket, view.ticketId))
            remainingTickets.push_back(std::move(storedTicket));
    }

    if (!remainingTickets.empty())
        return files.WriteFile(ticketFile, JoinStoredTickets(remainingTickets));

    Result<void> deleted = files.Delete(ticketFile);
    if (deleted.Failed())
        return deleted;

    return RemoveFolderIfEmpty(TicketFamilyPath(view.titleId.highPart));
}

}
