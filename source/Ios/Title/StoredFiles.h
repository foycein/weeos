#pragma once

#include "Formats/Bytes.h"
#include "Formats/Ticket.h"
#include "Formats/TitleMetadata.h"
#include "Ios/IosError.h"
#include "Ios/Result.h"

#include <cstdint>
#include <vector>

namespace Ios
{

// Reading and cutting up the files of the title service. A file that cannot be read gives the error that the caller names:
// ShortRead for a file of the NAND that is damaged, TitleInvalidArgument or TicketInvalid for bytes that a program handed over.
Result<Formats::TitleMetadata> ParseTitleMetadata(Formats::ByteView bytes, IosError errorForUnreadableBytes);
Result<Formats::Ticket> ParseTicket(Formats::ByteView bytes, IosError errorForUnreadableBytes);

// A ticket file holds the tickets one after the other, each 0x2A4 bytes long.
constexpr std::size_t StoredTicketSize = 0x2A4;

// The tickets of a ticket file as they are stored, one entry for each. Bytes behind the last whole ticket are not a ticket.
std::vector<Formats::Bytes> SplitStoredTickets(Formats::ByteView fileBytes);

Formats::Bytes JoinStoredTickets(const std::vector<Formats::Bytes>& tickets);

// True when the stored ticket can be read and has this ticket id.
bool HasTicketId(const Formats::Bytes& storedTicket, std::uint64_t ticketId);

}
