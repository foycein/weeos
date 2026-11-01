#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

// The banner of a title's save data: the file banner.bin in the data folder of the title. Data Management shows it.
// It holds a title and a subtitle, a picture of 192 by 64 pixels and up to eight icons of 48 by 48 pixels that take turns.
// The pictures are in the texture format RGB5A3.
struct SaveBanner
{
    std::uint32_t flags = 0;

    // Two bits for each icon, the first icon in the lowest bits: how long the icon stays. Zero ends the row of icons.
    std::uint16_t iconSpeeds = 0;

    std::u16string title;
    std::u16string subtitle;
    Bytes picture;
    std::vector<Bytes> icons;
};

constexpr std::uint32_t SaveBannerPictureWidth = 192;
constexpr std::uint32_t SaveBannerPictureHeight = 64;
constexpr std::uint32_t SaveBannerIconSize = 48;

// The smallest banner has one icon, the biggest eight.
constexpr std::size_t SmallestSaveBannerSize = 0x72A0;
constexpr std::size_t BiggestSaveBannerSize = 0xF0A0;

// The flag of a save data that must not be copied.
constexpr std::uint32_t SaveBannerNoCopyFlag = 0x1;

// The flag of icons that go back and forth instead of starting again at the first one.
constexpr std::uint32_t SaveBannerBouncingIconsFlag = 0x10;

// Throws a FormatError when the file is not a save banner.
SaveBanner ReadSaveBanner(ByteView file);

}
