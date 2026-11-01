#include "Formats/PlayLogAttachment.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view Magic = "03_0";
constexpr std::string_view Description = "Play log";

constexpr std::size_t OlderAttachmentSize = 0x558;
constexpr Byte UsedMark = 1;

// A place holds the mark, seven bytes that are not used, and the play record.
constexpr std::size_t SlotPaddingSize = 7;

// A name holds the mark, one byte that is not used, and twelve characters.
constexpr std::size_t NamePaddingSize = 1;

void ReadPlays(ByteReader& reader, PlayLogAttachment& playLog)
{
    for (std::optional<PlayRecord>& play : playLog.plays)
    {
        Byte mark = reader.ReadByte();
        reader.Skip(SlotPaddingSize);
        ByteView recordBytes = reader.ReadBytes(PlayRecordFileSize);
        if (mark == UsedMark)
            play = ReadPlayRecord(recordBytes);
    }
    playLog.ticksOfOtherTitles = reader.ReadUint64();
}

void ReadNames(ByteReader& reader, PlayLogAttachment& playLog)
{
    for (auto& name : playLog.namesSentTo)
    {
        Byte mark = reader.ReadByte();
        reader.Skip(NamePaddingSize);
        std::array<char16_t, PlayLogNameLength> characters{};
        for (char16_t& character : characters)
            character = reader.ReadUint16();
        if (mark == UsedMark)
            name = characters;
    }
    playLog.moreNamesSentTo = reader.ReadUint32();
}

}

PlayLogAttachment ReadPlayLogAttachment(ByteView bytes)
{
    ByteReader reader(bytes, Description);
    PlayLogAttachment playLog;
    if (bytes.size() == OlderAttachmentSize)
    {
        ReadPlays(reader, playLog);
        return playLog;
    }

    if (bytes.size() != PlayLogAttachmentSize)
        reader.Fail(std::format("{} bytes instead of {}", bytes.size(), PlayLogAttachmentSize));
    if (reader.ReadText(Magic.size()) != Magic)
        reader.Fail("the play log does not start with 03_0");

    playLog.unusedWord = reader.ReadUint32();
    ReadPlays(reader, playLog);
    ReadNames(reader, playLog);
    return playLog;
}

Bytes WritePlayLogAttachment(const PlayLogAttachment& playLog)
{
    ByteWriter writer;
    writer.WriteBytes(ByteView(reinterpret_cast<const Byte*>(Magic.data()), Magic.size()));
    writer.WriteUint32(playLog.unusedWord);

    for (const std::optional<PlayRecord>& play : playLog.plays)
    {
        writer.WriteByte(play ? UsedMark : 0);
        writer.WriteZeros(SlotPaddingSize);
        if (play)
            writer.WriteBytes(WritePlayRecord(*play));
        else
            writer.WriteZeros(PlayRecordFileSize);
    }
    writer.WriteUint64(playLog.ticksOfOtherTitles);

    for (const auto& name : playLog.namesSentTo)
    {
        writer.WriteByte(name ? UsedMark : 0);
        writer.WriteZeros(NamePaddingSize);
        for (std::size_t index = 0; index < PlayLogNameLength; ++index)
            writer.WriteUint16(name ? (*name)[index] : 0);
    }
    writer.WriteUint32(playLog.moreNamesSentTo);
    return writer.GetBytes();
}

}
