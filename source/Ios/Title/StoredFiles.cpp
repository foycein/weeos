#include "Ios/Title/StoredFiles.h"

#include "Formats/FormatError.h"

namespace Ios
{

Result<Formats::TitleMetadata> ParseTitleMetadata(Formats::ByteView bytes, IosError errorForUnreadableBytes)
{
    try
    {
        return Formats::ReadTitleMetadata(bytes);
    }
    catch (const Formats::FormatError&)
    {
        return errorForUnreadableBytes;
    }
}

Result<Formats::Ticket> ParseTicket(Formats::ByteView bytes, IosError errorForUnreadableBytes)
{
    try
    {
        return Formats::ReadTicket(bytes);
    }
    catch (const Formats::FormatError&)
    {
        return errorForUnreadableBytes;
    }
}

std::vector<Formats::Bytes> SplitStoredTickets(Formats::ByteView fileBytes)
{
    std::vector<Formats::Bytes> tickets;

    std::size_t ticketCount = fileBytes.size() / StoredTicketSize;
    for (std::size_t ticketNumber = 0; ticketNumber < ticketCount; ++ticketNumber)
    {
        Formats::ByteView ticket = fileBytes.subspan(ticketNumber * StoredTicketSize, StoredTicketSize);
        tickets.emplace_back(ticket.begin(), ticket.end());
    }

    return tickets;
}

Formats::Bytes JoinStoredTickets(const std::vector<Formats::Bytes>& tickets)
{
    Formats::Bytes fileBytes;
    for (const Formats::Bytes& ticket : tickets)
        fileBytes.insert(fileBytes.end(), ticket.begin(), ticket.end());

    return fileBytes;
}

bool HasTicketId(const Formats::Bytes& storedTicket, std::uint64_t ticketId)
{
    Result<Formats::Ticket> ticket = ParseTicket(storedTicket, IosError::TicketInvalid);
    return ticket.Succeeded() && ticket.Value().ticketId == ticketId;
}

}
