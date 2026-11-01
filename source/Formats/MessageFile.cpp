#include "Formats/MessageFile.h"

#include "Formats/ByteReader.h"
#include "Formats/SectionedFile.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "Message file";
constexpr std::size_t TagSize = 4;
constexpr std::string_view FileTag = "MESG";
constexpr std::string_view TypeTag = "bmg1";
constexpr std::uint32_t SectionCountWithoutIds = 2;
constexpr std::uint32_t SectionCountWithIds = 3;
constexpr Byte Utf16Encoding = 2;
constexpr std::size_t FileHeaderSize = 0x20;
constexpr std::uint16_t SupportedEntrySize = 4;
constexpr std::size_t UnusedAfterEntrySize = 4;
constexpr std::size_t UnusedAfterIdCountSize = 6;
constexpr int EscapeSizeShift = 8;
constexpr std::size_t EscapeHeaderBytes = 4;

ByteView ReadSectionBody(ByteReader& reader, std::string_view expectedTag)
{
    std::size_t sectionStart = reader.Position();
    std::string tag = reader.ReadText(TagSize);
    if (tag != expectedTag)
        reader.Fail(std::format("the section {} was expected, but the section {} comes next", expectedTag, tag));

    std::uint32_t size = reader.ReadUint32();
    if (size < SectionHeaderSize)
        reader.Fail(std::format("the section {} has the size {}, which is less than its header", tag, size));

    reader.MoveTo(sectionStart);
    ByteView section = reader.ReadBytes(size);
    return section.subspan(SectionHeaderSize);
}

// Gives the number of sections.
std::uint32_t ReadFileHeader(ByteReader& reader)
{
    std::string fileTag = reader.ReadText(TagSize);
    std::string typeTag = reader.ReadText(TagSize);
    if (fileTag != FileTag || typeTag != TypeTag)
        reader.Fail(std::format("the tags {} and {} are not {} and {}", fileTag, typeTag, FileTag, TypeTag));

    std::uint32_t fileSize = reader.ReadUint32();
    if (fileSize != reader.Size())
        reader.Fail(std::format("the header says the file has {} bytes, but it has {}", fileSize, reader.Size()));

    std::uint32_t sectionCount = reader.ReadUint32();
    if (sectionCount != SectionCountWithoutIds && sectionCount != SectionCountWithIds)
        reader.Fail(std::format("the file has {} sections, but only files with the sections INF1, DAT1 and maybe MID1 are supported", sectionCount));

    Byte encoding = reader.ReadByte();
    if (encoding != Utf16Encoding)
        reader.Fail(std::format("the encoding {} is not supported, only UTF-16 is", encoding));

    reader.MoveTo(FileHeaderSize);
    return sectionCount;
}

// The index (section INF1) has one entry for each message. An entry is the offset of the text in the text pool.
std::vector<std::uint32_t> ReadTextOffsets(ByteView indexBody)
{
    ByteReader reader(indexBody, DataDescription);
    std::uint16_t messageCount = reader.ReadUint16();
    std::uint16_t entrySize = reader.ReadUint16();
    if (entrySize != SupportedEntrySize)
        reader.Fail(std::format("the entries of the message index have {} bytes, but only {} are supported", entrySize, SupportedEntrySize));

    reader.Skip(UnusedAfterEntrySize);

    std::vector<std::uint32_t> textOffsets;
    for (std::uint16_t messageNumber = 0; messageNumber < messageCount; ++messageNumber)
        textOffsets.push_back(reader.ReadUint32());

    return textOffsets;
}

// The rest of an escape sequence, after the escape character: its size and group, its kind and its parameters.
std::u16string ReadEscapeSequence(ByteReader& reader)
{
    char16_t sizeAndGroup = reader.ReadUint16();
    std::size_t size = sizeAndGroup >> EscapeSizeShift;
    if (size < EscapeHeaderBytes || size % sizeof(char16_t) != 0)
        reader.Fail(std::format("an escape sequence has the size {}", size));

    std::u16string sequence(1, sizeAndGroup);
    std::size_t remainingCharacters = (size - EscapeHeaderBytes) / sizeof(char16_t);
    for (std::size_t characterNumber = 0; characterNumber < remainingCharacters; ++characterNumber)
        sequence.push_back(reader.ReadUint16());

    return sequence;
}

std::u16string ReadMessage(ByteView textPool, std::uint32_t textOffset)
{
    ByteReader reader(textPool, DataDescription);
    reader.MoveTo(textOffset);

    std::u16string message;
    while (true)
    {
        char16_t character = reader.ReadUint16();
        if (character == 0)
            return message;

        message.push_back(character);
        if (character == MessageEscapeCharacter)
            message += ReadEscapeSequence(reader);
    }
}

// The ids section has the number of messages and the ids, one for each message in the same order.
std::vector<std::uint32_t> ReadMessageIds(ByteView idsBody, std::size_t messageCount)
{
    ByteReader reader(idsBody, DataDescription);
    std::uint16_t idCount = reader.ReadUint16();
    if (idCount != messageCount)
        reader.Fail(std::format("the file has {} message ids for {} messages", idCount, messageCount));

    reader.Skip(UnusedAfterIdCountSize);
    std::vector<std::uint32_t> ids;
    for (std::uint16_t idNumber = 0; idNumber < idCount; ++idNumber)
        ids.push_back(reader.ReadUint32());

    return ids;
}

}

MessageFile ReadMessageFile(ByteView bytes)
{
    ByteReader reader(bytes, DataDescription);
    std::uint32_t sectionCount = ReadFileHeader(reader);

    ByteView indexBody = ReadSectionBody(reader, "INF1");
    ByteView textPool = ReadSectionBody(reader, "DAT1");

    MessageFile file;
    for (std::uint32_t textOffset : ReadTextOffsets(indexBody))
        file.messages.push_back(ReadMessage(textPool, textOffset));

    if (sectionCount == SectionCountWithIds)
        file.messageIds = ReadMessageIds(ReadSectionBody(reader, "MID1"), file.messages.size());

    return file;
}

}
