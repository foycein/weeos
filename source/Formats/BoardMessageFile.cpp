#include "Formats/BoardMessageFile.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/Crc32.h"
#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view Magic = "RI_5";
constexpr std::string_view Description = "Board message";

constexpr std::size_t CheckedHeaderSize = 0x140;
constexpr std::size_t HeaderPaddingSize = 4;
constexpr std::size_t TextAlignment = 8;
constexpr std::size_t MiiAreaSize = 0x4C;
constexpr std::size_t AttachmentAlignment = 0x20;

std::size_t AlignUp(std::size_t value, std::size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

std::size_t TextAreaSize(const std::u16string& text)
{
    return AlignUp((text.size() + 1) * sizeof(char16_t), TextAlignment);
}

std::u16string ReadTextAt(ByteView bytes, std::uint32_t offset)
{
    if (offset == 0)
        return {};

    ByteReader reader(bytes, Description);
    reader.MoveTo(offset);
    std::u16string text;
    while (true)
    {
        char16_t character = reader.ReadUint16();
        if (character == 0)
            return text;
        text.push_back(character);
    }
}

void WriteText(ByteWriter& writer, const std::u16string& text)
{
    std::size_t start = writer.GetBytes().size();
    for (char16_t character : text)
        writer.WriteUint16(character);
    writer.WriteZeros(start + TextAreaSize(text) - writer.GetBytes().size());
}

struct AttachmentPlace
{
    std::uint32_t kind = 0;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

// Everything of the header but the CRC and the padding behind it.
Bytes WriteCheckedHeader(const BoardMessage& message, const std::array<std::uint32_t, 3>& textAndMiiOffsets,
    const std::array<AttachmentPlace, BoardMessageAttachmentCount>& attachmentPlaces)
{
    ByteWriter header;
    header.WriteBytes(ByteView(reinterpret_cast<const Byte*>(Magic.data()), Magic.size()));
    header.WriteFloat(message.positionX);
    header.WriteFloat(message.positionY);
    header.WriteByte(message.optOutFlag);
    header.WriteBytes(message.unusedKindBytes);
    header.WriteByte(static_cast<Byte>(message.kind));
    header.WriteUint64(message.boardTime);
    header.WriteBytes(message.sender);
    header.WriteUint16(static_cast<std::uint16_t>(message.senderKind));
    header.WriteUint16(message.cannotBeAnswered);
    for (std::uint32_t offset : textAndMiiOffsets)
        header.WriteUint32(offset);
    for (const AttachmentPlace& place : attachmentPlaces)
    {
        header.WriteUint32(place.kind);
        header.WriteUint32(place.offset);
        header.WriteUint32(place.size);
    }
    return header.GetBytes();
}

}

BoardMessage ReadBoardMessage(ByteView bytes)
{
    ByteReader reader(bytes, Description);
    if (reader.ReadText(Magic.size()) != Magic)
        reader.Fail("the message does not start with RI_5");

    std::uint32_t storedCrc = ByteReader(bytes.subspan(CheckedHeaderSize), Description).ReadUint32();
    std::uint32_t crcOfHeader = ComputeCrc32(bytes.first(CheckedHeaderSize));
    if (storedCrc != crcOfHeader)
        reader.Fail(std::format("the CRC {:#x} is not the CRC {:#x} of the header", storedCrc, crcOfHeader));

    BoardMessage message;
    message.positionX = reader.ReadFloat();
    message.positionY = reader.ReadFloat();
    message.optOutFlag = reader.ReadByte();
    message.unusedKindBytes = reader.ReadByteArray<2>();
    message.kind = static_cast<BoardMessageKind>(reader.ReadByte());
    message.boardTime = reader.ReadUint64();
    message.sender = reader.ReadByteArray<BoardMessageSenderSize>();
    message.senderKind = static_cast<BoardSenderKind>(reader.ReadUint16());
    message.cannotBeAnswered = reader.ReadUint16();

    std::uint32_t subjectOffset = reader.ReadUint32();
    std::uint32_t bodyOffset = reader.ReadUint32();
    std::uint32_t miiOffset = reader.ReadUint32();
    message.subject = ReadTextAt(bytes, subjectOffset);
    message.body = ReadTextAt(bytes, bodyOffset);
    if (miiOffset != 0)
    {
        ByteReader miiReader(bytes, Description);
        miiReader.MoveTo(miiOffset);
        message.mii = miiReader.ReadByteArray<BoardMessageMiiSize>();
    }

    for (std::optional<BoardAttachment>& attachment : message.attachments)
    {
        std::uint32_t kind = reader.ReadUint32();
        std::uint32_t offset = reader.ReadUint32();
        std::uint32_t size = reader.ReadUint32();
        if (size == 0)
            continue;
        if (!IsRangeInside(offset, size, bytes.size()))
            reader.Fail(std::format("an attachment of {} bytes at {:#x} lies outside the message", size, offset));

        ByteView contents = bytes.subspan(offset, size);
        attachment = BoardAttachment{static_cast<BoardAttachmentKind>(kind), Bytes(contents.begin(), contents.end())};
    }
    return message;
}

Bytes WriteBoardMessage(const BoardMessage& message)
{
    std::size_t place = BoardMessageHeaderSize;
    std::array<std::uint32_t, 3> textAndMiiOffsets{};
    if (!message.subject.empty())
    {
        textAndMiiOffsets[0] = static_cast<std::uint32_t>(place);
        place += TextAreaSize(message.subject);
    }
    if (!message.body.empty())
    {
        textAndMiiOffsets[1] = static_cast<std::uint32_t>(place);
        place += TextAreaSize(message.body);
    }
    if (message.mii)
    {
        textAndMiiOffsets[2] = static_cast<std::uint32_t>(place);
        place += MiiAreaSize;
    }

    std::size_t firstAttachmentOffset = AlignUp(place, AttachmentAlignment);
    place = firstAttachmentOffset;
    std::array<AttachmentPlace, BoardMessageAttachmentCount> attachmentPlaces{};
    for (std::size_t index = 0; index < BoardMessageAttachmentCount; ++index)
    {
        const std::optional<BoardAttachment>& attachment = message.attachments[index];
        if (!attachment)
            continue;

        std::uint32_t size = static_cast<std::uint32_t>(attachment->contents.size());
        attachmentPlaces[index] = AttachmentPlace{static_cast<std::uint32_t>(attachment->kind), static_cast<std::uint32_t>(place), size};
        place += AlignUp(size, AttachmentAlignment);
    }

    Bytes checkedHeader = WriteCheckedHeader(message, textAndMiiOffsets, attachmentPlaces);
    ByteWriter writer;
    writer.WriteBytes(checkedHeader);
    writer.WriteUint32(ComputeCrc32(checkedHeader));
    writer.WriteZeros(HeaderPaddingSize);
    if (!message.subject.empty())
        WriteText(writer, message.subject);
    if (!message.body.empty())
        WriteText(writer, message.body);
    if (message.mii)
    {
        writer.WriteBytes(*message.mii);
        writer.WriteZeros(MiiAreaSize - BoardMessageMiiSize);
    }
    writer.WriteZeros(firstAttachmentOffset - writer.GetBytes().size());

    for (const std::optional<BoardAttachment>& attachment : message.attachments)
    {
        if (!attachment)
            continue;
        writer.WriteBytes(attachment->contents);
        writer.WriteZeros(AlignUp(attachment->contents.size(), AttachmentAlignment) - attachment->contents.size());
    }
    return writer.GetBytes();
}

}
