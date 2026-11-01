#include "Formats/SaveBanner.h"

#include "Formats/ByteReader.h"

#include <format>
#include <string_view>

namespace Formats
{

namespace
{

constexpr std::string_view Description = "Save banner";
constexpr std::string_view Tag = "WIBN";

constexpr std::size_t TitleOffset = 0x20;
constexpr std::size_t TextLength = 32;
constexpr std::size_t PictureOffset = 0xA0;
constexpr std::size_t BytesPerPixel = 2;
constexpr std::size_t PictureSize = SaveBannerPictureWidth * SaveBannerPictureHeight * BytesPerPixel;
constexpr std::size_t IconPictureSize = SaveBannerIconSize * SaveBannerIconSize * BytesPerPixel;

std::u16string ReadText(ByteReader& reader)
{
    std::u16string text;
    bool hasEnded = false;

    for (std::size_t index = 0; index < TextLength; ++index)
    {
        char16_t character = static_cast<char16_t>(reader.ReadUint16());
        if (character == 0)
            hasEnded = true;

        if (!hasEnded)
            text.push_back(character);
    }
    return text;
}

Bytes ReadPicture(ByteReader& reader, std::size_t size)
{
    ByteView picture = reader.ReadBytes(size);
    return Bytes(picture.begin(), picture.end());
}

}

SaveBanner ReadSaveBanner(ByteView file)
{
    ByteReader reader(file, Description);

    std::string tag = reader.ReadText(Tag.size());
    if (tag != Tag)
        reader.Fail(std::format("the tag {} is not {}", tag, Tag));

    if (file.size() < SmallestSaveBannerSize || file.size() > BiggestSaveBannerSize)
        reader.Fail(std::format("a banner has {:#x} to {:#x} bytes, this one {:#x}", SmallestSaveBannerSize, BiggestSaveBannerSize, file.size()));

    SaveBanner banner;
    banner.flags = reader.ReadUint32();
    banner.iconSpeeds = reader.ReadUint16();

    reader.MoveTo(TitleOffset);
    banner.title = ReadText(reader);
    banner.subtitle = ReadText(reader);

    reader.MoveTo(PictureOffset);
    banner.picture = ReadPicture(reader, PictureSize);

    while (reader.RemainingSize() >= IconPictureSize)
        banner.icons.push_back(ReadPicture(reader, IconPictureSize));

    return banner;
}

}
