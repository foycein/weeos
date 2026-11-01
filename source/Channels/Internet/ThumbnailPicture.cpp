#include "Channels/Internet/ThumbnailPicture.h"

#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"

#include <algorithm>

namespace Channels
{

namespace
{

constexpr std::uint16_t BitmapMark = 0x4D42;
constexpr std::uint32_t FileHeaderSize = 14;
constexpr std::uint32_t InformationHeaderSize = 40;
constexpr std::uint32_t PixelDataOffset = FileHeaderSize + InformationHeaderSize;
constexpr std::uint16_t PlaneCount = 1;
constexpr std::uint16_t BitsPerPixel = 24;
constexpr std::uint32_t BytesPerPixel = 3;
constexpr std::uint32_t RowAlignment = 4;

// 72 dots per inch, in dots per meter, as the reference console's pictures say.
constexpr std::uint32_t DotsPerMeter = 2834;

// The fields of the header that are always zero: the reserved field, no compression, and 0 for "all colors are used and important".
constexpr std::uint32_t ReservedField = 0;
constexpr std::uint32_t NoCompression = 0;
constexpr std::uint32_t AllColorsUsed = 0;
constexpr std::uint32_t AllColorsImportant = 0;

// Where the reader finds the fields it checks.
constexpr std::size_t PixelDataOffsetField = 10;
constexpr std::size_t WidthField = 18;
constexpr std::size_t BitsPerPixelField = 28;

// The bytes of a pixel of the file, and the alpha of every pixel of the picture.
constexpr std::size_t BlueByte = 0;
constexpr std::size_t GreenByte = 1;
constexpr std::size_t RedByte = 2;
constexpr Formats::Byte OpaqueAlpha = 0xFF;
constexpr Formats::Rgba8Pixel White = {0xFF, 0xFF, 0xFF, OpaqueAlpha};

constexpr std::uint32_t RowSize = static_cast<std::uint32_t>(Formats::RoundUpToMultiple(ThumbnailWidth * BytesPerPixel, RowAlignment));
constexpr std::uint32_t PixelDataSize = RowSize * ThumbnailHeight;

constexpr std::uint32_t LowHalfMask = 0xFFFF;
constexpr int BitsPerHalf = 16;

void AppendLittle16(Formats::Bytes& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<Formats::Byte>(value & Formats::ByteMask));
    bytes.push_back(static_cast<Formats::Byte>(value >> Formats::BitsPerByte));
}

void AppendLittle32(Formats::Bytes& bytes, std::uint32_t value)
{
    AppendLittle16(bytes, static_cast<std::uint16_t>(value & LowHalfMask));
    AppendLittle16(bytes, static_cast<std::uint16_t>(value >> BitsPerHalf));
}

// The average of the page's pixels that fall on one pixel of the thumbnail.
Formats::Rgba8Pixel AverageOf(const Formats::RgbaImage& picture, std::uint32_t left, std::uint32_t top, std::uint32_t right, std::uint32_t bottom)
{
    std::uint32_t red = 0;
    std::uint32_t green = 0;
    std::uint32_t blue = 0;
    std::uint32_t count = 0;
    for (std::uint32_t y = top; y < bottom; ++y)
    {
        for (std::uint32_t x = left; x < right; ++x)
        {
            const Formats::Rgba8Pixel& pixel = picture.pixels[static_cast<std::size_t>(y) * picture.width + x];
            red += pixel.red;
            green += pixel.green;
            blue += pixel.blue;
            ++count;
        }
    }

    if (count == 0)
        return White;

    Formats::Byte averageRed = static_cast<Formats::Byte>(red / count);
    Formats::Byte averageGreen = static_cast<Formats::Byte>(green / count);
    Formats::Byte averageBlue = static_cast<Formats::Byte>(blue / count);
    return {averageRed, averageGreen, averageBlue, OpaqueAlpha};
}

}

Formats::RgbaImage MakeThumbnail(const Formats::RgbaImage& pagePicture)
{
    std::uint32_t cropWidth = std::min(pagePicture.width, pagePicture.height * ThumbnailWidth / ThumbnailHeight);
    std::uint32_t cropHeight = std::min(pagePicture.height, cropWidth * ThumbnailHeight / ThumbnailWidth);

    Formats::RgbaImage thumbnail{ThumbnailWidth, ThumbnailHeight, std::vector<Formats::Rgba8Pixel>(ThumbnailWidth * ThumbnailHeight)};
    for (std::uint32_t y = 0; y < ThumbnailHeight; ++y)
    {
        std::uint32_t top = y * cropHeight / ThumbnailHeight;
        std::uint32_t bottom = std::max(top + 1, (y + 1) * cropHeight / ThumbnailHeight);
        for (std::uint32_t x = 0; x < ThumbnailWidth; ++x)
        {
            std::uint32_t left = x * cropWidth / ThumbnailWidth;
            std::uint32_t right = std::max(left + 1, (x + 1) * cropWidth / ThumbnailWidth);
            std::uint32_t rightInPicture = std::min(right, pagePicture.width);
            std::uint32_t bottomInPicture = std::min(bottom, pagePicture.height);
            thumbnail.pixels[static_cast<std::size_t>(y) * ThumbnailWidth + x] = AverageOf(pagePicture, left, top, rightInPicture, bottomInPicture);
        }
    }
    return thumbnail;
}

Formats::Bytes WriteThumbnailBitmap(const Formats::RgbaImage& thumbnail)
{
    Formats::Bytes bytes;
    AppendLittle16(bytes, BitmapMark);
    AppendLittle32(bytes, PixelDataOffset + PixelDataSize);
    AppendLittle32(bytes, ReservedField);
    AppendLittle32(bytes, PixelDataOffset);

    AppendLittle32(bytes, InformationHeaderSize);
    AppendLittle32(bytes, ThumbnailWidth);
    AppendLittle32(bytes, ThumbnailHeight);
    AppendLittle16(bytes, PlaneCount);
    AppendLittle16(bytes, BitsPerPixel);
    AppendLittle32(bytes, NoCompression);
    AppendLittle32(bytes, PixelDataSize);
    AppendLittle32(bytes, DotsPerMeter);
    AppendLittle32(bytes, DotsPerMeter);
    AppendLittle32(bytes, AllColorsUsed);
    AppendLittle32(bytes, AllColorsImportant);

    for (std::uint32_t row = 0; row < ThumbnailHeight; ++row)
    {
        std::uint32_t y = ThumbnailHeight - 1 - row;
        for (std::uint32_t x = 0; x < ThumbnailWidth; ++x)
        {
            const Formats::Rgba8Pixel& pixel = thumbnail.pixels[static_cast<std::size_t>(y) * ThumbnailWidth + x];
            bytes.push_back(pixel.blue);
            bytes.push_back(pixel.green);
            bytes.push_back(pixel.red);
        }
        bytes.resize(bytes.size() + RowSize - ThumbnailWidth * BytesPerPixel, 0);
    }
    return bytes;
}

std::optional<Formats::RgbaImage> ReadThumbnailBitmap(Formats::ByteView bytes)
{
    if (bytes.size() < PixelDataOffset + PixelDataSize)
        return std::nullopt;

    Formats::ByteReader reader(bytes, "the picture of a favorite");
    if (reader.ReadLittleEndianUint16() != BitmapMark)
        return std::nullopt;

    reader.MoveTo(PixelDataOffsetField);
    std::uint32_t dataOffset = reader.ReadLittleEndianUint32();
    reader.MoveTo(WidthField);
    std::uint32_t width = reader.ReadLittleEndianUint32();
    std::uint32_t height = reader.ReadLittleEndianUint32();
    reader.MoveTo(BitsPerPixelField);
    std::uint16_t bitsPerPixel = reader.ReadLittleEndianUint16();

    bool hasThumbnailSize = width == ThumbnailWidth && height == ThumbnailHeight && bitsPerPixel == BitsPerPixel;
    bool hasAllPixels = dataOffset + PixelDataSize <= bytes.size();
    if (!hasThumbnailSize || !hasAllPixels)
        return std::nullopt;

    Formats::RgbaImage thumbnail{ThumbnailWidth, ThumbnailHeight, std::vector<Formats::Rgba8Pixel>(ThumbnailWidth * ThumbnailHeight)};
    for (std::uint32_t row = 0; row < ThumbnailHeight; ++row)
    {
        std::uint32_t y = ThumbnailHeight - 1 - row;
        for (std::uint32_t x = 0; x < ThumbnailWidth; ++x)
        {
            std::size_t offset = dataOffset + static_cast<std::size_t>(row) * RowSize + x * BytesPerPixel;
            Formats::Rgba8Pixel pixel = {bytes[offset + RedByte], bytes[offset + GreenByte], bytes[offset + BlueByte], OpaqueAlpha};
            thumbnail.pixels[static_cast<std::size_t>(y) * ThumbnailWidth + x] = pixel;
        }
    }
    return thumbnail;
}

}
