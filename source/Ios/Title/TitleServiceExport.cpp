#include "Ios/Title/TitleService.h"

#include "Formats/FormatError.h"
#include "Formats/TitleEncryption.h"
#include "Ios/Title/StoredFiles.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>

namespace Ios
{

Result<Formats::AesKey> TitleService::ReadTitleKey(TitleId titleId) const
{
    Result<std::vector<Formats::Ticket>> tickets = ReadTickets(titleId);
    if (tickets.Failed())
        return tickets.Error();

    if (tickets.Value().empty())
        return IosError::TicketNotFound;

    try
    {
        return Formats::DecryptTitleKey(tickets.Value().front());
    }
    catch (const Formats::FormatError&)
    {
        return IosError::TicketInvalid;
    }
}

Result<Formats::Bytes> TitleService::ExportContent(TitleId titleId, std::uint32_t contentId) const
{
    Result<Formats::TitleMetadata> metadata = ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return metadata.Error();

    auto record = std::find_if(metadata.Value().contents.begin(), metadata.Value().contents.end(),
        [contentId](const Formats::ContentRecord& candidate) { return candidate.id == contentId; });
    if (record == metadata.Value().contents.end())
        return IosError::TitleInvalidArgument;

    Result<Formats::AesKey> titleKey = ReadTitleKey(titleId);
    if (titleKey.Failed())
        return titleKey.Error();

    Result<ContentStream> stream = OpenContentRecord(metadata.Value(), record->index);
    if (stream.Failed())
        return stream.Error();

    Formats::Bytes content = stream.Value().Read(stream.Value().GetLength());
    return Formats::EncryptContent(content, titleKey.Value(), *record);
}

}
