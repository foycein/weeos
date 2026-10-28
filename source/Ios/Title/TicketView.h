#pragma once

#include "Formats/Ticket.h"
#include "Ios/Title/TitleId.h"

#include <array>
#include <cstdint>

namespace Ios
{

// The compact form of a ticket, 0xD8 bytes in the original. Programs ask for the views of a title, keep them,
// and hand one back to prove that they own the title: to launch it, to open a content of it, to delete the ticket.
// The view leaves out what only the title service needs: the signature, the issuer and the title key.
struct TicketView
{
    std::uint8_t formatVersion;
    std::uint64_t ticketId;

    // Zero for a ticket that works on every console.
    std::uint32_t consoleId;

    TitleId titleId;
    std::uint16_t accessMask;
    std::uint16_t titleVersion;
    std::uint32_t permittedTitlesMask;
    std::uint32_t permitMask;
    bool isTitleExportAllowed;
    std::array<Formats::Byte, Formats::TicketReservedSize> reserved;
    std::array<Formats::Byte, Formats::TicketContentAccessSize> contentAccess;
    std::array<Formats::TicketLimit, Formats::TicketLimitCount> limits;

    bool operator==(const TicketView&) const = default;
};

TicketView MakeTicketView(const Formats::Ticket& ticket);

}
