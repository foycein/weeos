#include "SystemMenu/InstalledTitleContents.h"

#include "SystemMenu/MenuError.h"

#include <format>

namespace SystemMenu
{

InstalledTitleContents::InstalledTitleContents(Ios::TitleService& service, Ios::TitleId installedTitleId)
    : titleService(service)
    , titleId(installedTitleId)
{
}

Formats::Bytes InstalledTitleContents::ReadContent(std::uint16_t contentIndex)
{
    Ios::Result<std::vector<Ios::TicketView>> ticketViews = titleService.ListTicketViews(titleId);
    if (ticketViews.Failed() || ticketViews.Value().empty())
        throw MenuError(std::format("The title {} has no ticket, so its contents cannot be read", titleId.ToString()));

    Ios::Result<Ios::ContentStream> opened = titleService.OpenContent(titleId, ticketViews.Value().front(), contentIndex);
    if (opened.Failed())
    {
        std::string message = std::format("The title service cannot open the content {} of the title {} (result code {})", contentIndex,
            titleId.ToString(), Ios::OriginalResultCode(opened.Error()));
        throw MenuError(message, FatalErrorOf(opened.Error()));
    }

    Ios::ContentStream& stream = opened.Value();
    return stream.Read(stream.GetLength());
}

}
