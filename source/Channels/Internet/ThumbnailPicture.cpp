#include "Channels/Internet/ThumbnailPicture.h"

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

constexpr std::uint32_t RowSize = (ThumbnailWidth * BytesPerPixel + RowAlignment - 1) / RowAlignment * RowAlignment;
constexpr std::uint32_t PixelDataSize = RowSize * ThumbnailHeight;

void AppendLittle16(Formats::Bytes& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<Formats::Byte>(value & Formats::ByteMask));
    bytes.push_back(static_cast<Formats::Byte>(value >> Formats::BitsPerByte));
}

void AppendLittle32(Formats::Bytes& bytes, std::uint32_t value)
{
    AppendLittle16(bytes, static_cast<std::uint16_t>(value & 0xFFFF));
    AppendLittle16(bytes, static_cast<std::uint16_t>(value >> 16));
}

std::uint32_t ReadLittle32(Formats::ByteView bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index)
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (Formats::BitsPerByte * index);
    return value;
}

std::uint16_t ReadLittle16(Formats::ByteView bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << Formats::BitsPerByte));
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
        return {255, 255, 255, 255};
    return {static_cast<Formats::Byte>(red / count), static_cast<Formats::Byte>(green / count), static_cast<Formats::Byte>(blue / count), 255};
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
            thumbnail.pixels[static_cast<std::size_t>(y) * ThumbnailWidth + x] =
                AverageOf(pagePicture, left, top, std::min(right, pagePicture.width), std::min(bottom, pagePicture.height));
        }
    }
    return thumbnail;
}

Formats::Bytes WriteThumbnailBitmap(const Formats::RgbaImage& thumbnail)
{
    Formats::Bytes bytes;
    AppendLittle16(bytes, BitmapMark);
    AppendLittle32(bytes, PixelDataOffset + PixelDataSize);
    AppendLittle32(bytes, 0);
    AppendLittle32(bytes, PixelDataOffset);

    AppendLittle32(bytes, InformationHeaderSize);
    AppendLittle32(bytes, ThumbnailWidth);
    AppendLittle32(bytes, ThumbnailHeight);
    AppendLittle16(bytes, PlaneCount);
    AppendLittle16(bytes, BitsPerPixel);
    AppendLittle32(bytes, 0);
    AppendLittle32(bytes, PixelDataSize);
    AppendLittle32(bytes, DotsPerMeter);
    AppendLittle32(bytes, DotsPerMeter);
    AppendLittle32(bytes, 0);
    AppendLittle32(bytes, 0);

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
    if (bytes.size() < PixelDataOffset + PixelDataSize || ReadLittle16(bytes, 0) != BitmapMark)
        return std::nullopt;

    std::uint32_t dataOffset = ReadLittle32(bytes, 10);
    bool isThumbnail = ReadLittle32(bytes, 18) == ThumbnailWidth && ReadLittle32(bytes, 22) == ThumbnailHeight &&
        ReadLittle16(bytes, 28) == BitsPerPixel && dataOffset + PixelDataSize <= bytes.size();
    if (!isThumbnail)
        return std::nullopt;

    Formats::RgbaImage thumbnail{ThumbnailWidth, ThumbnailHeight, std::vector<Formats::Rgba8Pixel>(ThumbnailWidth * ThumbnailHeight)};
    for (std::uint32_t row = 0; row < ThumbnailHeight; ++row)
    {
        std::uint32_t y = ThumbnailHeight - 1 - row;
        for (std::uint32_t x = 0; x < ThumbnailWidth; ++x)
        {
            std::size_t offset = dataOffset + static_cast<std::size_t>(row) * RowSize + x * BytesPerPixel;
            thumbnail.pixels[static_cast<std::size_t>(y) * ThumbnailWidth + x] = {bytes[offset + 2], bytes[offset + 1], bytes[offset], 255};
        }
    }
    return thumbnail;
}

}
