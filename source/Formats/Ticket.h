#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/Signature.h"

#include <array>
#include <cstdint>
#include <string>

namespace Formats
{

constexpr std::size_t TicketLimitCount = 8;
constexpr std::size_t TicketContentAccessSize = 64;
constexpr std::size_t TicketKeyExchangeDataSize = 60;
constexpr std::size_t TicketReservedSize = 48;

struct TicketLimit
{
    std::uint32_t type;
    std::uint32_t maximumUsage;

    bool operator==(const TicketLimit&) const = default;
};

// A ticket is the permission to use a title. It also carries the title key, protected by the common key.
struct Ticket
{
    Signature signature;
    std::string issuer;

    // Used only by tickets that are bound to one console.
    std::array<Byte, TicketKeyExchangeDataSize> keyExchangeData;

    std::uint8_t formatVersion;
    AesKey encryptedTitleKey;
    std::uint64_t ticketId;

    // Zero for a ticket that works on every console.
    std::uint32_t consoleId;

    std::uint64_t titleId;

    // Two bytes behind the title id. The tickets of Nintendo's titles hold 0xFFFF or 0x0001. The format documents no meaning for them.
    std::uint16_t accessMask;

    std::uint16_t titleVersion;
    std::uint32_t permittedTitlesMask;
    std::uint32_t permitMask;
    bool isTitleExportAllowed;

    // 0 is the common key, 1 the Korean common key and 2 the common key of the Wii mode of the Wii U.
    std::uint8_t commonKeyIndex;

    // Bytes behind the common key index. They are zero in almost every ticket; the ticket view passes them on as they are.
    std::array<Byte, TicketReservedSize> reserved;

    // One bit for each content of the title. The format documents no bit order, so the bytes are kept as they are.
    std::array<Byte, TicketContentAccessSize> contentAccess;

    std::array<TicketLimit, TicketLimitCount> limits;
};

Ticket ReadTicket(ByteView bytes);

}
