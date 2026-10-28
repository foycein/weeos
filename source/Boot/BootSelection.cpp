#include "Boot/BootSelection.h"

#include "Boot/BootError.h"
#include "Boot/TitleIds.h"
#include "Formats/FileBytes.h"
#include "Formats/Ticket.h"
#include "Formats/TitleMetadata.h"

#include <format>

namespace Boot
{

namespace
{

// The NAND folder has the names of the console: ticket/00000001/00000002.tik
std::filesystem::path TicketFilePath(const std::filesystem::path& nandFolder, std::uint64_t titleId)
{
    std::string fileName = std::format("{:08x}.tik", TitleIdLowPart(titleId));
    return nandFolder / "ticket" / std::format("{:08x}", TitleIdHighPart(titleId)) / fileName;
}

// The NAND folder has the names of the console: title/00000001/00000002/content/title.tmd
std::filesystem::path TitleMetadataFilePath(const std::filesystem::path& nandFolder, std::uint64_t titleId)
{
    std::filesystem::path titleFolder = nandFolder / "title" / std::format("{:08x}", TitleIdHighPart(titleId)) /
        std::format("{:08x}", TitleIdLowPart(titleId));
    return titleFolder / "content" / "title.tmd";
}

Formats::Bytes ReadNandFile(const std::filesystem::path& file, std::string_view description)
{
    if (!std::filesystem::is_regular_file(file))
        throw BootError(std::format("The NAND has no {}: {} is not there", description, file.string()));

    return Formats::ReadFileBytes(file);
}

// A ticket file holds one or more tickets. boot2 starts the title with the first one.
void RequireTicketOfTitle(const Ios::TicketView& firstTicketView, std::uint64_t titleId)
{
    if (firstTicketView.titleId.ToValue() != titleId)
    {
        throw BootError(std::format("The ticket for title {:016x} belongs to title {:016x}",
            titleId, firstTicketView.titleId.ToValue()));
    }
}

BootSelection DecideBootTitle(const Ios::TicketView& firstTicketView, const Formats::TitleMetadata& titleMetadata)
{
    RequireTicketOfTitle(firstTicketView, SystemMenuTitleId);

    if (!IsIosTitleId(titleMetadata.requiredSystemVersion))
    {
        throw BootError(std::format("The title metadata of title {:016x} needs {:016x}, which is not an IOS",
            SystemMenuTitleId, titleMetadata.requiredSystemVersion));
    }

    BootSelection selection;
    selection.titleId = SystemMenuTitleId;
    selection.titleVersion = titleMetadata.titleVersion;
    selection.iosTitleId = titleMetadata.requiredSystemVersion;
    selection.ticketView = firstTicketView;
    return selection;
}

}

BootSelection SelectBootTitle(const std::filesystem::path& nandFolder)
{
    Formats::Bytes ticketFile = ReadNandFile(TicketFilePath(nandFolder, SystemMenuTitleId), "ticket");
    Formats::Ticket firstTicket = Formats::ReadTicket(ticketFile);

    Formats::Bytes titleMetadataFile = ReadNandFile(TitleMetadataFilePath(nandFolder, SystemMenuTitleId), "title metadata");
    Formats::TitleMetadata titleMetadata = Formats::ReadTitleMetadata(titleMetadataFile);

    return DecideBootTitle(Ios::MakeTicketView(firstTicket), titleMetadata);
}

BootSelection SelectBootTitle(Ios::TitleService& titles)
{
    Ios::TitleId systemMenu = Ios::TitleId::FromValue(SystemMenuTitleId);

    Ios::Result<std::vector<Ios::TicketView>> ticketViews = titles.ListTicketViews(systemMenu);
    if (ticketViews.Failed())
        throw BootError("The ticket file of the System Menu cannot be read", ticketViews.Error());

    if (ticketViews.Value().empty())
        throw BootError("The NAND has no ticket for the System Menu");

    Ios::Result<Formats::TitleMetadata> titleMetadata = titles.ReadTitleMetadata(systemMenu);
    if (titleMetadata.Failed())
        throw BootError("The NAND has no title metadata for the System Menu", titleMetadata.Error());

    return DecideBootTitle(ticketViews.Value().front(), titleMetadata.Value());
}

}
