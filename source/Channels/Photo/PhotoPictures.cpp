#include "Channels/Photo/PhotoPictures.h"

#include "Media/AviReader.h"
#include "Media/ExifReader.h"
#include "Media/JpegDecoder.h"
#include "Media/MovieFrames.h"
#include "Media/PictureShapes.h"
#include "Media/QuickTimeReader.h"

#include "Formats/FormatError.h"

namespace Channels
{

namespace
{

constexpr int LargestShrinkFactor = 8;
constexpr int SizeRounding = 4;
constexpr int QuarterTurnsInATurn = 4;
constexpr int TurnRight = 1;
constexpr int TurnUpsideDown = 2;
constexpr int TurnLeft = 3;

int RoundUpToFour(int size)
{
    return (size + SizeRounding - 1) / SizeRounding * SizeRounding;
}

std::optional<Formats::RgbaImage> DecodeToFit(Formats::ByteView jpegBytes, int largestWidth, int largestHeight)
{
    Media::JpegInformation information = Media::ReadJpegInformation(jpegBytes);
    if (IsTooThin(information.width, information.height))
        return std::nullopt;

    std::optional<int> factor = ShrinkFactorToFit(information.width, information.height, largestWidth, largestHeight);
    if (!factor)
        return std::nullopt;

    return Media::ShrinkPicture(Media::DecodeJpeg(jpegBytes), *factor);
}

std::optional<Formats::RgbaImage> FirstFrameOfMovie(Formats::ByteView fileBytes, PhotoFileKind kind)
{
    std::optional<Media::Movie> movie = ReadPlayableMovie(fileBytes, kind);
    if (!movie || movie->video->frames.empty())
        return std::nullopt;

    const Media::MoviePiece& first = movie->video->frames.front();
    if (!Formats::IsRangeInside(first.place, first.size, fileBytes.size()))
        return std::nullopt;

    Formats::RgbaImage frame = Media::DecodeMovieFrame(*movie->video, fileBytes.subspan(static_cast<std::size_t>(first.place), first.size));
    std::optional<int> factor = ShrinkFactorToFit(static_cast<int>(frame.width), static_cast<int>(frame.height), ThumbnailLargestWidth,
        ThumbnailLargestHeight);
    return Media::ShrinkPicture(frame, factor.value_or(LargestShrinkFactor));
}

}

std::optional<int> ShrinkFactorToFit(int width, int height, int largestWidth, int largestHeight)
{
    for (int factor = 1; factor <= LargestShrinkFactor; factor *= 2)
    {
        int shrunkWidth = RoundUpToFour((width + factor - 1) / factor);
        int shrunkHeight = RoundUpToFour((height + factor - 1) / factor);
        if (shrunkWidth <= largestWidth && shrunkHeight <= largestHeight)
            return factor;
    }
    return std::nullopt;
}

bool IsTooThin(int width, int height)
{
    if (width <= 0 || height <= 0)
        return true;

    return width / height >= ThinnestRatio || height / width >= ThinnestRatio;
}

int QuarterTurnsOfOrientation(int exifOrientation)
{
    if (exifOrientation == static_cast<int>(Media::ExifOrientation::TurnedRight))
        return TurnRight;

    if (exifOrientation == static_cast<int>(Media::ExifOrientation::TurnedLeft))
        return TurnLeft;

    return 0;
}

Formats::RgbaImage TurnByQuarterTurns(const Formats::RgbaImage& picture, int quarterTurns)
{
    int turns = ((quarterTurns % QuarterTurnsInATurn) + QuarterTurnsInATurn) % QuarterTurnsInATurn;
    if (turns == TurnRight)
        return Media::TurnPictureRight(picture);

    if (turns == TurnUpsideDown)
        return Media::TurnPictureUpsideDown(picture);

    if (turns == TurnLeft)
        return Media::TurnPictureLeft(picture);

    return picture;
}

std::optional<Formats::Bytes> ReadCardFile(Ios::SdCardService& sdCard, const Ios::SdPath& path)
{
    Ios::Result<Ios::SdFile> file = sdCard.Open(path, Ios::OpenMode::Read);
    if (file.Failed())
        return std::nullopt;

    Ios::Result<std::uint32_t> length = file.Value().GetLength();
    if (length.Failed())
        return std::nullopt;

    Ios::Result<Formats::Bytes> bytes = file.Value().Read(length.Value());
    file.Value().Close();
    if (bytes.Failed())
        return std::nullopt;

    return bytes.Value();
}

std::optional<Formats::RgbaImage> MakeThumbnail(Formats::ByteView fileBytes, PhotoFileKind kind)
{
    try
    {
        if (kind != PhotoFileKind::Photo)
            return FirstFrameOfMovie(fileBytes, kind);

        Media::ExifInformation exif = Media::ReadExifInformation(fileBytes);
        std::optional<Formats::RgbaImage> thumbnail;
        if (exif.thumbnail)
            thumbnail = DecodeToFit(*exif.thumbnail, ThumbnailLargestWidth, ThumbnailLargestHeight);
        if (!thumbnail)
            thumbnail = DecodeToFit(fileBytes, ThumbnailLargestWidth, ThumbnailLargestHeight);
        if (!thumbnail)
            return std::nullopt;

        return TurnByQuarterTurns(*thumbnail, QuarterTurnsOfOrientation(exif.orientation));
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

std::optional<Formats::RgbaImage> MakeShownPhoto(Formats::ByteView fileBytes)
{
    try
    {
        std::optional<Formats::RgbaImage> photo = DecodeToFit(fileBytes, PictureLargestSide, PictureLargestSide);
        if (!photo)
            return std::nullopt;

        Media::ExifInformation exif = Media::ReadExifInformation(fileBytes);
        return TurnByQuarterTurns(*photo, QuarterTurnsOfOrientation(exif.orientation));
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

std::optional<Media::Movie> ReadPlayableMovie(Formats::ByteView fileBytes, PhotoFileKind kind)
{
    try
    {
        Media::Movie movie = kind == PhotoFileKind::AviMovie ? Media::ReadAviMovie(fileBytes) : Media::ReadQuickTimeMovie(fileBytes);
        if (!movie.video || movie.video->kind == Media::MovieVideoKind::Unsupported)
            return std::nullopt;

        bool hasFittingSize = movie.video->width > 0 && movie.video->width <= MovieLargestWidth && movie.video->height > 0
            && movie.video->height / movie.video->fieldCount <= MovieLargestHeight;
        if (!hasFittingSize || IsTooThin(movie.video->width, movie.video->height))
            return std::nullopt;

        return movie;
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

}
