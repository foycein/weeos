#include "Sound/BannerSound.h"

#include "Sound/StreamFile.h"

#include "Formats/ByteReader.h"

#include <format>
#include <string_view>

namespace Sound
{

namespace
{

constexpr std::string_view Description = "Banner sound";
constexpr std::string_view WrapperTag = "IMD5";
constexpr std::size_t WrapperSize = 32;

}

DecodedWave ReadBannerSound(Formats::ByteView soundFile)
{
    Formats::ByteReader wrapper(soundFile, Description);
    std::string tag = wrapper.ReadText(WrapperTag.size());
    if (tag != WrapperTag)
        wrapper.Fail(std::format("the tag {} is not {}", tag, WrapperTag));

    wrapper.MoveTo(WrapperSize);
    return ReadStreamFile(soundFile.subspan(WrapperSize));
}

}
