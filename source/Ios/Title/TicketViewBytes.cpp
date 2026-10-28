#include "Ios/Title/TicketViewBytes.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"

namespace Ios
{

namespace
{

constexpr std::size_t UnusedAfterFormatVersionSize = 3;
constexpr std::size_t UnusedAfterExportFlagSize = 1;
constexpr std::size_t UnusedAfterContentAccessSize = 2;

}

Formats::Bytes WriteTicketView(const TicketView& view)
{
    Formats::ByteWriter writer;
    writer.WriteByte(view.formatVersion);
    writer.WriteZeros(UnusedAfterFormatVersionSize);
    writer.WriteUint64(view.ticketId);
    writer.WriteUint32(view.consoleId);
    writer.WriteUint64(view.titleId.ToValue());
    writer.WriteUint16(view.accessMask);
    writer.WriteUint16(view.titleVersion);
    writer.WriteUint32(view.permittedTitlesMask);
    writer.WriteUint32(view.permitMask);
    writer.WriteByte(view.isTitleExportAllowed ? 1 : 0);
    writer.WriteZeros(UnusedAfterExportFlagSize);
    writer.WriteBytes(view.reserved);
    writer.WriteBytes(view.contentAccess);
    writer.WriteZeros(UnusedAfterContentAccessSize);

    for (const Formats::TicketLimit& limit : view.limits)
    {
        writer.WriteUint32(limit.type);
        writer.WriteUint32(limit.maximumUsage);
    }

    return writer.GetBytes();
}

Result<TicketView> ParseTicketView(Formats::ByteView bytes, IosError errorForUnreadableBytes)
{
    if (bytes.size() != TicketViewSize)
        return errorForUnreadableBytes;

    Formats::ByteReader reader(bytes, "Ticket view");

    TicketView view;
    view.formatVersion = reader.ReadByte();
    reader.Skip(UnusedAfterFormatVersionSize);
    view.ticketId = reader.ReadUint64();
    view.consoleId = reader.ReadUint32();
    view.titleId = TitleId::FromValue(reader.ReadUint64());
    view.accessMask = reader.ReadUint16();
    view.titleVersion = reader.ReadUint16();
    view.permittedTitlesMask = reader.ReadUint32();
    view.permitMask = reader.ReadUint32();
    view.isTitleExportAllowed = reader.ReadByte() != 0;
    reader.Skip(UnusedAfterExportFlagSize);
    view.reserved = reader.ReadByteArray<Formats::TicketReservedSize>();
    view.contentAccess = reader.ReadByteArray<Formats::TicketContentAccessSize>();
    reader.Skip(UnusedAfterContentAccessSize);

    for (Formats::TicketLimit& limit : view.limits)
    {
        limit.type = reader.ReadUint32();
        limit.maximumUsage = reader.ReadUint32();
    }

    return view;
}

}
