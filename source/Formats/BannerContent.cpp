#include "Formats/BannerContent.h"

#include "Formats/ByteReader.h"
#include "Formats/Lz77.h"

#include <format>
#include <string_view>

namespace Formats
{

namespace
{

constexpr std::string_view BannerDescription = "Banner content";
constexpr std::string_view LayoutFileDescription = "Banner layout file";

// The header starts at byte 0x40. Its first 0x40 bytes are empty, then the tag "IMET" follows.
constexpr std::size_t HeaderOffset = 0x40;
constexpr std::size_t TagOffset = 0x80;
constexpr std::string_view Tag = "IMET";
constexpr std::size_t TagSize = 4;

// Behind the tag: the size of the header (the U8 archive starts behind it), then three numbers that are not used here
// (the version and the sizes of icon.bin, banner.bin and sound.bin), and then the names.
constexpr std::size_t NamesOffset = TagOffset + 0x1C;

// Every language has 42 characters in two lines of 21.
constexpr std::size_t LineLength = 21;

constexpr std::string_view WrapperTag = "IMD5";
constexpr std::size_t WrapperSize = 32;

std::u16string ReadLine(ByteReader& reader)
{
    std::u16string line;
    bool hasEnded = false;

    for (std::size_t index = 0; index < LineLength; ++index)
    {
        char16_t character = static_cast<char16_t>(reader.ReadUint16());
        if (character == 0)
            hasEnded = true;

        if (!hasEnded)
            line.push_back(character);
    }
    return line;
}

ChannelName ReadName(ByteReader& reader)
{
    ChannelName name;
    name.firstLine = ReadLine(reader);
    name.secondLine = ReadLine(reader);
    return name;
}

}

BannerContent ReadBannerContent(ByteView content)
{
    ByteReader reader(content, BannerDescription);

    reader.MoveTo(TagOffset);
    std::string tag = reader.ReadText(TagSize);
    if (tag != Tag)
        reader.Fail(std::format("the tag {} is not {}", tag, Tag));

    std::uint32_t headerSize = reader.ReadUint32();
    std::size_t archiveOffset = HeaderOffset + headerSize;
    if (archiveOffset > content.size())
        reader.Fail(std::format("the header says the archive starts at {:#x}, behind the end of the content", archiveOffset));

    BannerContent banner;
    reader.MoveTo(NamesOffset);
    for (ChannelName& name : banner.names)
        name = ReadName(reader);

    banner.files = ReadU8Archive(content.subspan(archiveOffset));
    return banner;
}

U8Archive ReadLayoutFile(ByteView wrappedFile)
{
    ByteReader reader(wrappedFile, LayoutFileDescription);

    std::string tag = reader.ReadText(WrapperTag.size());
    if (tag != WrapperTag)
        reader.Fail(std::format("the tag {} is not {}", tag, WrapperTag));

    if (wrappedFile.size() < WrapperSize)
        reader.Fail("the file is shorter than its header");

    return ReadU8Archive(DecompressLz77(wrappedFile.subspan(WrapperSize)));
}

}
