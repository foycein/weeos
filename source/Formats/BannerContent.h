#pragma once

#include "Formats/Bytes.h"
#include "Formats/U8Archive.h"

#include <array>
#include <cstddef>
#include <string>

namespace Formats
{

// The console has ten languages. They are numbered as the system configuration numbers them: Japanese 0, English 1, German 2,
// French 3, Spanish 4, Italian 5, Dutch 6, simplified Chinese 7, traditional Chinese 8, Korean 9.
constexpr std::size_t BannerLanguageCount = 10;
constexpr std::size_t EnglishLanguageNumber = 1;

// A channel writes its name in every language, on up to two lines.
struct ChannelName
{
    std::u16string firstLine;
    std::u16string secondLine;
};

// The first content of a channel title is its banner. A header comes first, with the names of the channel in the ten languages.
// A U8 archive follows with the files of the banner: meta/icon.bin (the picture of the tile in the channel grid),
// meta/banner.bin (the picture of the start window) and meta/sound.bin.
struct BannerContent
{
    std::array<ChannelName, BannerLanguageCount> names;

    // True when another content of the title holds a script or a module that drives the icon (icon.cs or icon.rso),
    // and the same for the banner (banner.cs or banner.rso).
    bool isIconDrivenByCode = false;
    bool isBannerDrivenByCode = false;

    U8Archive files;
};

// Throws a FormatError when the content is not a banner.
BannerContent ReadBannerContent(ByteView content);

// The layouts of an icon or a banner: icon.bin and banner.bin start with a header of 32 bytes (IMD5), and an LZ77 compressed
// U8 archive follows. The archive holds the folders arc/blyt, arc/anim and arc/timg.
// Throws a FormatError when the file does not start with the header or the data behind it is damaged.
U8Archive ReadLayoutFile(ByteView wrappedFile);

}
