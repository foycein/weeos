#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

#include <optional>
#include <string_view>

namespace Formats
{

// The photo that the Photo Channel posts to the Wii Menu, in its data folder: the icon (160 x 120) and the channel preview (480 x 272).
constexpr std::string_view PostedIconPicturePath = "/title/00010002/48415941/data/noerase/photo_i.dat";
constexpr std::string_view PostedPreviewPicturePath = "/title/00010002/48415941/data/noerase/photo_b.dat";
constexpr int PostedIconWidth = 160;
constexpr int PostedIconHeight = 120;
constexpr int PostedPreviewWidth = 480;
constexpr int PostedPreviewHeight = 272;

// The size of such a file: a header of 0x20 bytes and two bytes a pixel.
std::size_t PostedPictureFileSize(int width, int height);

// The file of a picture (the Photo Channel's 0x80009E20 to 0x8000A138): the code HAYA, the size of the file, the CRC-32 of everything
// after it, width and height, zeros up to 0x20, and the picture as a GX texture of RGB565 in tiles of 4 x 4.
Bytes MakePostedPictureFile(const RgbaImage& picture);

// The picture of such a file; nothing for an empty file (all zeros, nothing posted) or one whose header or check number does not fit.
std::optional<RgbaImage> ReadPostedPictureFile(ByteView bytes, int width, int height);

}
