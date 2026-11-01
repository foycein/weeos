#include "Sound/SoundFileHeader.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::size_t TagSize = 4;
constexpr std::uint16_t BigEndianByteOrderMark = 0xFEFF;
constexpr std::size_t SectionTableOffset = 0x10;

Formats::ByteView CutOutSection(Formats::ByteReader& reader, Formats::ByteView fileBytes, std::string_view expectedTag)
{
    std::uint32_t offset = reader.ReadUint32();
    std::uint32_t size = reader.ReadUint32();
    if (!Formats::IsRangeInside(offset, size, fileBytes.size()))
        reader.Fail(std::format("the section {} lies outside the file", expectedTag));

    Formats::ByteView section = fileBytes.subspan(offset, size);
    Formats::ByteReader sectionReader(section, expectedTag);
    std::string tag = sectionReader.ReadText(TagSize);
    if (tag != expectedTag)
        reader.Fail(std::format("the section at {:#x} has the tag {} instead of {}", offset, tag, expectedTag));

    std::uint32_t sizeInSection = sectionReader.ReadUint32();
    if (sizeInSection != size)
        reader.Fail(std::format("the section {} says it has {} bytes, but the header says {}", expectedTag, sizeInSection, size));

    return section;
}

const KnownVersion* FindKnownVersion(std::span<const KnownVersion> knownVersions, std::uint16_t version)
{
    for (const KnownVersion& known : knownVersions)
    {
        if (known.version == version)
            return &known;
    }
    return nullptr;
}

std::string ListVersions(std::span<const KnownVersion> knownVersions)
{
    std::string list;
    for (const KnownVersion& known : knownVersions)
        list += std::format("{}{:#x}", list.empty() ? "" : ", ", known.version);
    return list;
}

}

SoundFile ReadSoundFileHeader(Formats::ByteView bytes, std::string_view expectedTag, std::span<const KnownVersion> knownVersions,
    std::size_t expectedHeaderSize, std::string_view dataDescription)
{
    Formats::ByteReader reader(bytes, dataDescription);

    std::string tag = reader.ReadText(TagSize);
    if (tag != expectedTag)
        reader.Fail(std::format("the tag {} is not {}", tag, expectedTag));

    std::uint16_t byteOrderMark = reader.ReadUint16();
    if (byteOrderMark != BigEndianByteOrderMark)
        reader.Fail(std::format("the byte order mark {:#x} is not {:#x}, only big endian files are supported",
            byteOrderMark, BigEndianByteOrderMark));

    SoundFile file;
    file.version = reader.ReadUint16();
    const KnownVersion* known = FindKnownVersion(knownVersions, file.version);
    if (known == nullptr)
        reader.Fail(std::format("the version {:#x} is not supported, the known versions are {}", file.version, ListVersions(knownVersions)));

    std::uint32_t fileSize = reader.ReadUint32();
    if (fileSize != bytes.size())
        reader.Fail(std::format("the header says the file has {} bytes, but it has {}", fileSize, bytes.size()));

    std::uint16_t headerSize = reader.ReadUint16();
    if (headerSize != expectedHeaderSize)
        reader.Fail(std::format("the header size {:#x} is not {:#x}", headerSize, expectedHeaderSize));

    std::uint16_t sectionCount = reader.ReadUint16();
    if (sectionCount != known->sectionTags.size())
        reader.Fail(std::format("the file has {} sections instead of {}", sectionCount, known->sectionTags.size()));

    reader.MoveTo(SectionTableOffset);
    for (std::string_view sectionTag : known->sectionTags)
        file.sections.push_back(CutOutSection(reader, bytes, sectionTag));

    return file;
}

}
