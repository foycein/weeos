#pragma once

#include "Channels/Photo/PhotoLibrary.h"

#include "Media/Movie.h"

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

#include <optional>

namespace Channels
{

// The sizes the channel decodes to (0x8001267C, 0x800129B4, 0x80012260): a photo is decoded at a scale of 1, 1/2, 1/4 or 1/8, the
// first scale whose size, rounded up to a multiple of 4, fits. A photo that does not fit even at 1/8 is not shown.
constexpr int ThumbnailLargestWidth = 160;
constexpr int ThumbnailLargestHeight = 120;
constexpr int PictureLargestSide = 1024;

// A frame of a movie may be 848 x 480 at most (0x800178A0), each field of it 480 lines.
constexpr int MovieLargestWidth = 848;
constexpr int MovieLargestHeight = 480;

// A picture one of whose sides is 1000 times the other or more is "too thin" and not shown (0x80014ACC).
constexpr int ThinnestRatio = 1000;

// The shrink factor (1, 2, 4 or 8) that makes the size fit, or nothing when 1/8 is still too large.
std::optional<int> ShrinkFactorToFit(int width, int height, int largestWidth, int largestHeight);

bool IsTooThin(int width, int height);

// How many quarter turns to the right the channel turns a photo by the Exif orientation: 6 one, 8 three. Everything else,
// also 3 (upside down) and the mirrored values, is shown as stored, as the original does (0x80024AB4).
int QuarterTurnsOfOrientation(int exifOrientation);

Formats::RgbaImage TurnByQuarterTurns(const Formats::RgbaImage& picture, int quarterTurns);

// The whole file. Nothing when the card or the file cannot be read.
std::optional<Formats::Bytes> ReadCardFile(Ios::SdCardService& sdCard, const Ios::SdPath& path);

// The small picture of the thumbnail list: the Exif thumbnail of a photo when it has a usable one, else the photo itself;
// the first frame of a movie. Nothing for a file whose format the channel does not support.
std::optional<Formats::RgbaImage> MakeThumbnail(Formats::ByteView fileBytes, PhotoFileKind kind);

// A photo as the channel shows it, turned by its orientation. Nothing when its format is not supported.
std::optional<Formats::RgbaImage> MakeShownPhoto(Formats::ByteView fileBytes);

// The first frame of a movie at its own size. Nothing when the channel cannot play its video.
std::optional<Formats::RgbaImage> FirstFrameOfMovie(Formats::ByteView fileBytes, PhotoFileKind kind);

// A black picture of one pixel, used where a photo cannot be decoded.
Formats::RgbaImage MissingPhotoPicture();

// The movie of a file, when the channel can play its video.
std::optional<Media::Movie> ReadPlayableMovie(Formats::ByteView fileBytes, PhotoFileKind kind);

}
