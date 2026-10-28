#include "Formats/SectionedFile.h"

#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t TagSize = 4;
constexpr std::uint16_t BigEndianByteOrderMark = 0xFEFF;
constexpr std::uint16_t ExpectedHeaderSize = 0x10;

Section ReadSection(ByteReader& reader)
{
    std::size_t sectionStart = reader.Position();
    std::string tag = reader.ReadText(TagSize);
    std::uint32_t size = reader.ReadUint32();
    if (size < SectionHeaderSize)
        reader.Fail(std::format("the section {} has the size {}, which is less than its header", tag, size));

    reader.MoveTo(sectionStart);
    return {tag, reader.ReadBytes(size)};
}

}

SectionedFile ReadSectionedFile(ByteView bytes, std::string_view expectedTag, std::string_view dataDescription)
{
    ByteReader reader(bytes, dataDescription);

    std::string tag = reader.ReadText(TagSize);
    if (tag != expectedTag)
        reader.Fail(std::format("the tag {} is not {}", tag, expectedTag));

    std::uint16_t byteOrderMark = reader.ReadUint16();
    if (byteOrderMark != BigEndianByteOrderMark)
        reader.Fail(std::format("the byte order mark {:#x} is not {:#x}, only big endian files are supported",
            byteOrderMark, BigEndianByteOrderMark));

    SectionedFile file;
    file.version = reader.ReadUint16();

    std::uint32_t fileSize = reader.ReadUint32();
    if (fileSize != bytes.size())
        reader.Fail(std::format("the header says the file has {} bytes, but it has {}", fileSize, bytes.size()));

    std::uint16_t headerSize = reader.ReadUint16();
    if (headerSize != ExpectedHeaderSize)
        reader.Fail(std::format("the header size {:#x} is not {:#x}", headerSize, ExpectedHeaderSize));

    std::uint16_t sectionCount = reader.ReadUint16();
    for (std::uint16_t sectionNumber = 0; sectionNumber < sectionCount; ++sectionNumber)
        file.sections.push_back(ReadSection(reader));

    if (!reader.IsAtEnd())
        reader.Fail(std::format("{} bytes are left after the last section", reader.RemainingSize()));

    return file;
}

ByteReader OpenSection(const Section& section, std::string_view dataDescription)
{
    ByteReader reader(section.bytes, std::format("{} section {}", dataDescription, section.tag));
    reader.Skip(SectionHeaderSize);
    return reader;
}

SectionCursor::SectionCursor(const std::vector<Section>& sectionsToWalk, std::string_view dataDescription)
    : sections(sectionsToWalk)
    , description(dataDescription)
{
}

bool SectionCursor::IsAtEnd() const
{
    return nextIndex == sections.size();
}

bool SectionCursor::NextIs(std::string_view tag) const
{
    return !IsAtEnd() && sections[nextIndex].tag == tag;
}

const Section& SectionCursor::Take()
{
    if (IsAtEnd())
        throw FormatError(std::format("{}: a section is missing at the end of the file", description));

    return sections[nextIndex++];
}

const Section& SectionCursor::Take(std::string_view tag)
{
    if (IsAtEnd())
        throw FormatError(std::format("{}: the section {} is missing at the end of the file", description, tag));

    if (!NextIs(tag))
    {
        throw FormatError(std::format("{}: the section {} was expected, but the section {} comes next", description, tag,
            sections[nextIndex].tag));
    }

    return sections[nextIndex++];
}

}
