#include "SystemMenu/OwnTitleContents.h"

#include "SystemMenu/MenuError.h"

#include <format>

namespace SystemMenu
{

OwnTitleContents::OwnTitleContents(Ios::TitleService& service)
    : titleService(service)
{
}

Formats::Bytes OwnTitleContents::ReadContent(std::uint16_t contentIndex)
{
    Ios::Result<Ios::ContentStream> opened = titleService.OpenOwnContent(contentIndex);
    if (opened.Failed())
    {
        std::string message = std::format("The title service cannot open the content {} of the running title (result code {})",
            contentIndex, Ios::OriginalResultCode(opened.Error()));
        throw MenuError(message, FatalErrorOf(opened.Error()));
    }

    Ios::ContentStream& stream = opened.Value();
    return stream.Read(stream.GetLength());
}

}
