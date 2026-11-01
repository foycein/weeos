#include "Formats/NumberedMessageFile.h"

#include "Formats/ByteReader.h"

#include <format>
#include <optional>
#include <vector>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "Numbered message file";
constexpr std::size_t TagSize = 4;
constexpr std::string_view FileTag = "MESG";
constexpr std::string_view TypeTag = "bmg1";
constexpr std::size_t SectionCountPosition = 0x0C;
constexpr std::size_t EncodingPosition = 0x10;
constexpr Byte Utf16Encoding = 2;
constexpr std::size_t FileHeaderSize = 0x20;
constexpr std::size_t SectionHeaderSize = 8;

// The index (INF1): a count and the size of an entry, then the entries, each starting with the offset of its text.
constexpr std::size_t IndexEntriesPosition = 8;

// The numbers (MID1): a count, then the number of every message in the order of the index.
constexpr std::size_t NumbersPosition = 8;

// An escape sequence is the character 0x1A, then a byte with the length of the whole sequence in bytes.
constexpr char16_t EscapeCharacter = 0x001A;

struct Sections
{
    std::optional<ByteView> index;
    std::optional<ByteView> textPool;
    std::optional<ByteView> numbers;
};

Sections FindSections(ByteReader& reader, std::uint32_t sectionCount)
{
    Sections sections;
    reader.MoveTo(FileHeaderSize);
    for (std::uint32_t sectionNumber = 0; sectionNumber < sectionCount; ++sectionNumber)
    {
        std::size_t sectionStart = reader.Position();
        std::string tag = reader.ReadText(TagSize);
        std::uint32_t size = reader.ReadUint32();
        if (size < SectionHeaderSize)
            reader.Fail(std::format("the section {} has the size {}, which is less than its header", tag, size));

        reader.MoveTo(sectionStart);
        ByteView section = reader.ReadBytes(size);
        if (tag == "INF1")
            sections.index = section;
        else if (tag == "DAT1")
            sections.textPool = section.subspan(SectionHeaderSize);
        else if (tag == "MID1")
            sections.numbers = section;
    }
    return sections;
}

std::u16string ReadText(ByteView textPool, std::uint32_t textOffset)
{
    ByteReader reader(textPool, DataDescription);
    reader.MoveTo(textOffset);

    std::u16string text;
    while (true)
    {
        std::size_t characterStart = reader.Position();
        char16_t character = reader.ReadUint16();
        if (character == 0)
            return text;

        if (character != EscapeCharacter)
        {
            text.push_back(character);
            continue;
        }

        Byte sequenceLength = reader.ReadByte();
        reader.MoveTo(characterStart + sequenceLength);
    }
}

}

NumberedMessageFile ReadNumberedMessageFile(ByteView bytes)
{
    ByteReader reader(bytes, DataDescription);
    std::string fileTag = reader.ReadText(TagSize);
    std::string typeTag = reader.ReadText(TagSize);
    if (fileTag != FileTag || typeTag != TypeTag)
        reader.Fail(std::format("the tags {} and {} are not {} and {}", fileTag, typeTag, FileTag, TypeTag));

    reader.MoveTo(SectionCountPosition);
    std::uint32_t sectionCount = reader.ReadUint32();
    reader.MoveTo(EncodingPosition);
    if (reader.ReadByte() != Utf16Encoding)
        reader.Fail("only UTF-16 texts are supported");

    Sections sections = FindSections(reader, sectionCount);
    if (!sections.index || !sections.textPool || !sections.numbers)
        reader.Fail("the file needs the sections INF1, DAT1 and MID1");

    ByteReader index(*sections.index, DataDescription);
    index.MoveTo(SectionHeaderSize);
    std::uint16_t messageCount = index.ReadUint16();
    std::uint16_t entrySize = index.ReadUint16();

    ByteReader numbers(*sections.numbers, DataDescription);
    numbers.MoveTo(SectionHeaderSize);
    if (numbers.ReadUint16() != messageCount)
        reader.Fail("the numbers and the index list a different count of messages");

    NumberedMessageFile file;
    for (std::uint16_t messageIndex = 0; messageIndex < messageCount; ++messageIndex)
    {
        index.MoveTo(SectionHeaderSize + IndexEntriesPosition + std::size_t{messageIndex} * entrySize);
        std::uint32_t textOffset = index.ReadUint32();

        numbers.MoveTo(SectionHeaderSize + NumbersPosition + std::size_t{messageIndex} * sizeof(std::uint32_t));
        std::uint32_t number = numbers.ReadUint32();

        file.messages[number] = ReadText(*sections.textPool, textOffset);
    }
    return file;
}

}
