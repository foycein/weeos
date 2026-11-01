#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

#include <cstdint>
#include <optional>

namespace Channels
{

// The picture of a favorite: 90 x 68 pixels, as the Internet Channel keeps it in bmarks.bmp (each place holds a Windows bitmap of this
// size with 24 bits a pixel, found in the reference console's bmarks.bmp) and as favorites.css shows it.
constexpr std::uint32_t ThumbnailWidth = 90;
constexpr std::uint32_t ThumbnailHeight = 68;

// The top left part of the page with the shape of a thumbnail, made smaller by averaging the pixels that fall on each pixel of it.
Formats::RgbaImage MakeThumbnail(const Formats::RgbaImage& pagePicture);

// A Windows bitmap file: the header of 54 bytes, then the rows from the bottom up, blue, green and red, each row filled to 4 bytes.
Formats::Bytes WriteThumbnailBitmap(const Formats::RgbaImage& thumbnail);

// Nothing when the bytes are no bitmap of 90 x 68 pixels with 24 bits a pixel.
std::optional<Formats::RgbaImage> ReadThumbnailBitmap(Formats::ByteView bytes);

}
