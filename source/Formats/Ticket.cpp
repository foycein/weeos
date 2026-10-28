#include "Formats/Ticket.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t IssuerSize = 64;
constexpr std::size_t UnusedAfterFormatVersionSize = 2;
constexpr std::size_t UnusedAfterTitleKeySize = 1;
constexpr std::size_t UnusedAfterContentAccessSize = 2;

constexpr std::uint8_t SupportedFormatVersion = 0;

TicketLimit ReadTicketLimit(ByteReader& reader)
{
    TicketLimit limit;
    limit.type = reader.ReadUint32();
    limit.maximumUsage = reader.ReadUint32();
    return limit;
}

}

Ticket ReadTicket(ByteView bytes)
{
    ByteReader reader(bytes, "Ticket");

    Ticket ticket;
    ticket.signature = ReadSignature(reader);
    ticket.issuer = reader.ReadText(IssuerSize);
    ticket.keyExchangeData = reader.ReadByteArray<TicketKeyExchangeDataSize>();
    ticket.formatVersion = reader.ReadByte();
    if (ticket.formatVersion != SupportedFormatVersion)
        reader.Fail(std::format("the ticket format version {} is not supported", ticket.formatVersion));

    reader.Skip(UnusedAfterFormatVersionSize);
    ticket.encryptedTitleKey = reader.ReadByteArray<AesBlockSize>();
    reader.Skip(UnusedAfterTitleKeySize);
    ticket.ticketId = reader.ReadUint64();
    ticket.consoleId = reader.ReadUint32();
    ticket.titleId = reader.ReadUint64();
    ticket.accessMask = reader.ReadUint16();
    ticket.titleVersion = reader.ReadUint16();
    ticket.permittedTitlesMask = reader.ReadUint32();
    ticket.permitMask = reader.ReadUint32();
    ticket.isTitleExportAllowed = reader.ReadByte() != 0;
    ticket.commonKeyIndex = reader.ReadByte();
    ticket.reserved = reader.ReadByteArray<TicketReservedSize>();
    ticket.contentAccess = reader.ReadByteArray<TicketContentAccessSize>();
    reader.Skip(UnusedAfterContentAccessSize);

    for (TicketLimit& limit : ticket.limits)
        limit = ReadTicketLimit(reader);

    return ticket;
}

}
