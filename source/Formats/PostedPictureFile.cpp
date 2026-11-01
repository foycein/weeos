#include "Formats/PostedPictureFile.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/Crc32.h"
#include "Formats/TextureDecoder.h"

namespace Formats
{

namespace
{

constexpr std::size_t PictureHeaderSize = 0x20;
constexpr std::size_t BytesPerRgb565Pixel = 2;
constexpr std::uint32_t PictureFileCode = 0x48415941;
constexpr std::size_t CheckedPartStart = 12;
constexpr std::size_t HeaderFieldsSize = 20;

// RGB565: five bits of red, six of green, five of blue. A GX texture keeps the pixels in tiles of 4 x 4, row by row.
constexpr int TileSide = 4;
constexpr int RedShift = 11;
constexpr int GreenShift = 5;
constexpr int FiveBitDrop = 3;
constexpr int SixBitDrop = 2;

std::uint16_t ToRgb565(const Rgba8Pixel& pixel)
{
    int red = pixel.red >> FiveBitDrop;
    int green = pixel.green >> SixBitDrop;
    int blue = pixel.blue >> FiveBitDrop;
    return static_cast<std::uint16_t>((red << RedShift) | (green << GreenShift) | blue);
}

void WriteTile(ByteWriter& pixels, const RgbaImage& picture, std::uint32_t tileLeft, std::uint32_t tileTop)
{
    for (std::uint32_t y = tileTop; y < tileTop + TileSide; ++y)
    {
        for (std::uint32_t x = tileLeft; x < tileLeft + TileSide; ++x)
        {
            std::size_t pixelIndex = static_cast<std::size_t>(y) * picture.width + x;
            pixels.WriteUint16(ToRgb565(picture.pixels[pixelIndex]));
        }
    }
}

}

std::size_t PostedPictureFileSize(int width, int height)
{
    return PictureHeaderSize + static_cast<std::size_t>(width) * height * BytesPerRgb565Pixel;
}

Bytes MakePostedPictureFile(const RgbaImage& picture)
{
    ByteWriter pixels;
    for (std::uint32_t tileTop = 0; tileTop < picture.height; tileTop += TileSide)
    {
        for (std::uint32_t tileLeft = 0; tileLeft < picture.width; tileLeft += TileSide)
            WriteTile(pixels, picture, tileLeft, tileTop);
    }

    ByteWriter checkedPart;
    checkedPart.WriteUint32(picture.width);
    checkedPart.WriteUint32(picture.height);
    checkedPart.WriteZeros(PictureHeaderSize - HeaderFieldsSize);
    checkedPart.WriteBytes(pixels.GetBytes());

    ByteWriter file;
    file.WriteUint32(PictureFileCode);
    file.WriteUint32(static_cast<std::uint32_t>(PictureHeaderSize + pixels.GetBytes().size()));
    file.WriteUint32(ComputeCrc32(checkedPart.GetBytes()));
    file.WriteBytes(checkedPart.GetBytes());
    return file.GetBytes();
}

std::optional<RgbaImage> ReadPostedPictureFile(ByteView bytes, int width, int height)
{
    if (bytes.size() != PostedPictureFileSize(width, height))
        return std::nullopt;

    ByteReader reader(bytes, "posted picture");
    std::uint32_t fileCode = reader.ReadUint32();
    std::uint32_t fileSize = reader.ReadUint32();
    std::uint32_t checksum = reader.ReadUint32();
    std::uint32_t storedWidth = reader.ReadUint32();
    std::uint32_t storedHeight = reader.ReadUint32();

    bool isRightFile = fileCode == PictureFileCode && fileSize == bytes.size();
    bool hasRightSize = storedWidth == static_cast<std::uint32_t>(width) && storedHeight == static_cast<std::uint32_t>(height);
    bool hasRightChecksum = checksum == ComputeCrc32(bytes.subspan(CheckedPartStart));
    if (!isRightFile || !hasRightSize || !hasRightChecksum)
        return std::nullopt;

    return DecodeTexture(TextureFormat::Rgb565, width, height, bytes.subspan(PictureHeaderSize), std::nullopt);
}

}
