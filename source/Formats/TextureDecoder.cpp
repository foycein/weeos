#include "Formats/TextureDecoder.h"

#include "Formats/Alignment.h"
#include "Formats/BitFields.h"
#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <array>
#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t LargestBlockPixelCount = 64;
constexpr Byte OpaqueAlpha = 255;

constexpr int NibbleBitCount = 4;
constexpr unsigned NibbleMask = 0x0F;
constexpr std::size_t PixelsPerSharedByte = 2;
constexpr std::uint16_t PaletteIndexMask14Bits = 0x3FFF;

constexpr std::size_t SubBlockSide = 4;
constexpr std::size_t SubBlockSize = 8;
constexpr std::size_t SubBlocksPerBlock = 4;
constexpr std::size_t SubBlocksPerRow = 2;
constexpr std::size_t CompressedBlockSide = 8;
constexpr std::size_t CompressedColorCount = 4;
constexpr int BitsPerPixelIndex = 2;

// The two bytes of a pixel that are stored together, alpha and red or green and blue.
constexpr std::size_t Rgba8PairSize = 2;
constexpr std::size_t Rgba8GreenBlueOffset = 32;

using BlockPixels = std::array<Rgba8Pixel, LargestBlockPixelCount>;
using Color16Decoder = Rgba8Pixel (*)(std::uint16_t);

// Fills the eight bits of a byte by repeating the bits of the value, so that the largest value gives 255 (Likely).
Byte ExpandToEightBits(unsigned value, int bitCount)
{
    unsigned expanded = 0;
    int filledBitCount = 0;

    while (filledBitCount < BitsPerByte)
    {
        expanded = (expanded << bitCount) | value;
        filledBitCount += bitCount;
    }

    return static_cast<Byte>(expanded >> (filledBitCount - BitsPerByte));
}

Rgba8Pixel MakeIntensityPixel(Byte intensity)
{
    return {intensity, intensity, intensity, intensity};
}

Rgba8Pixel MakeIntensityAlphaPixel(Byte intensity, Byte alpha)
{
    return {intensity, intensity, intensity, alpha};
}

// The high byte is the alpha and the low byte the intensity.
Rgba8Pixel DecodeIntensityAlpha8Color(std::uint16_t color)
{
    Byte alpha = static_cast<Byte>(color >> BitsPerByte);
    Byte intensity = static_cast<Byte>(color & ByteMask);
    return MakeIntensityAlphaPixel(intensity, alpha);
}

// Where a color channel lies inside a 16 bit color.
struct ChannelBits
{
    int lowestBit;
    int bitCount;
};

// RGB565 colors have the bits RRRRRGGGGGGBBBBB.
constexpr ChannelBits Rgb565Red = {11, 5};
constexpr ChannelBits Rgb565Green = {5, 6};
constexpr ChannelBits Rgb565Blue = {0, 5};

// RGB5A3 colors have one of two layouts. The top bit tells which.
constexpr ChannelBits Rgb5A3OpaqueFlag = {15, 1};

// The layout without alpha is 1RRRRRGGGGGBBBBB.
constexpr ChannelBits Rgb555Red = {10, 5};
constexpr ChannelBits Rgb555Green = {5, 5};
constexpr ChannelBits Rgb555Blue = {0, 5};

// The layout with alpha is 0AAARRRRGGGGBBBB.
constexpr ChannelBits Rgb4A3Alpha = {12, 3};
constexpr ChannelBits Rgb4A3Red = {8, 4};
constexpr ChannelBits Rgb4A3Green = {4, 4};
constexpr ChannelBits Rgb4A3Blue = {0, 4};

Byte ExtractChannel(std::uint16_t color, ChannelBits channel)
{
    return ExpandToEightBits(ExtractBits(color, channel.lowestBit, channel.bitCount), channel.bitCount);
}

Rgba8Pixel DecodeRgb565Color(std::uint16_t color)
{
    Rgba8Pixel pixel;
    pixel.red = ExtractChannel(color, Rgb565Red);
    pixel.green = ExtractChannel(color, Rgb565Green);
    pixel.blue = ExtractChannel(color, Rgb565Blue);
    pixel.alpha = OpaqueAlpha;
    return pixel;
}

Rgba8Pixel DecodeRgb5A3Color(std::uint16_t color)
{
    bool isOpaque = ExtractBits(color, Rgb5A3OpaqueFlag.lowestBit, Rgb5A3OpaqueFlag.bitCount) != 0;

    Rgba8Pixel pixel;
    if (isOpaque)
    {
        pixel.alpha = OpaqueAlpha;
        pixel.red = ExtractChannel(color, Rgb555Red);
        pixel.green = ExtractChannel(color, Rgb555Green);
        pixel.blue = ExtractChannel(color, Rgb555Blue);
        return pixel;
    }

    pixel.alpha = ExtractChannel(color, Rgb4A3Alpha);
    pixel.red = ExtractChannel(color, Rgb4A3Red);
    pixel.green = ExtractChannel(color, Rgb4A3Green);
    pixel.blue = ExtractChannel(color, Rgb4A3Blue);
    return pixel;
}

Color16Decoder ColorDecoderOf(PaletteFormat format)
{
    switch (format)
    {
    case PaletteFormat::IntensityAlpha8:
        return DecodeIntensityAlpha8Color;

    case PaletteFormat::Rgb565:
        return DecodeRgb565Color;

    case PaletteFormat::Rgb5A3:
        return DecodeRgb5A3Color;
    }

    throw FormatError(std::format("The palette format {} is not known", static_cast<std::uint32_t>(format)));
}

std::vector<Rgba8Pixel> DecodePalette(const TexturePalette& palette)
{
    Color16Decoder decodeColor = ColorDecoderOf(palette.format);

    ByteReader reader(palette.entries, "Palette");
    std::vector<Rgba8Pixel> colors;
    for (std::size_t entry = 0; entry < palette.entryCount; ++entry)
        colors.push_back(decodeColor(reader.ReadUint16()));

    return colors;
}

// Two pixels share a byte. The first pixel is in the high bits.
unsigned ReadNibble(ByteView block, std::size_t pixelIndex)
{
    Byte sharedByte = block[pixelIndex / PixelsPerSharedByte];
    bool isFirstPixel = pixelIndex % PixelsPerSharedByte == 0;
    if (isFirstPixel)
        return sharedByte >> NibbleBitCount;

    return sharedByte & NibbleMask;
}

const Rgba8Pixel& LookUpColor(const std::vector<Rgba8Pixel>& paletteColors, std::size_t paletteIndex)
{
    if (paletteIndex >= paletteColors.size())
    {
        throw FormatError(std::format("Texture: a pixel uses the palette entry {}, but the palette has {} entries",
            paletteIndex, paletteColors.size()));
    }

    return paletteColors[paletteIndex];
}

void DecodeIntensity4Block(ByteView block, std::size_t pixelCount, BlockPixels& pixels)
{
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
        pixels[pixelIndex] = MakeIntensityPixel(ExpandToEightBits(ReadNibble(block, pixelIndex), NibbleBitCount));
}

void DecodeIntensity8Block(ByteView block, std::size_t pixelCount, BlockPixels& pixels)
{
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
        pixels[pixelIndex] = MakeIntensityPixel(block[pixelIndex]);
}

// One byte for each pixel: the high four bits are the alpha and the low four bits the intensity.
void DecodeIntensityAlpha4Block(ByteView block, std::size_t pixelCount, BlockPixels& pixels)
{
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
    {
        Byte intensity = ExpandToEightBits(block[pixelIndex] & NibbleMask, NibbleBitCount);
        Byte alpha = ExpandToEightBits(block[pixelIndex] >> NibbleBitCount, NibbleBitCount);
        pixels[pixelIndex] = MakeIntensityAlphaPixel(intensity, alpha);
    }
}

void Decode16BitBlock(ByteView block, std::size_t pixelCount, Color16Decoder decodeColor, BlockPixels& pixels)
{
    ByteReader reader(block, "Texture block");
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
        pixels[pixelIndex] = decodeColor(reader.ReadUint16());
}

// The block holds the alpha and red bytes of all pixels first, then the green and blue bytes of all pixels.
void DecodeRgba8Block(ByteView block, std::size_t pixelCount, BlockPixels& pixels)
{
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
    {
        std::size_t alphaRedOffset = pixelIndex * Rgba8PairSize;
        std::size_t greenBlueOffset = Rgba8GreenBlueOffset + pixelIndex * Rgba8PairSize;

        pixels[pixelIndex].alpha = block[alphaRedOffset];
        pixels[pixelIndex].red = block[alphaRedOffset + 1];
        pixels[pixelIndex].green = block[greenBlueOffset];
        pixels[pixelIndex].blue = block[greenBlueOffset + 1];
    }
}

void DecodePalette4Block(ByteView block, std::size_t pixelCount, const std::vector<Rgba8Pixel>& paletteColors, BlockPixels& pixels)
{
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
        pixels[pixelIndex] = LookUpColor(paletteColors, ReadNibble(block, pixelIndex));
}

void DecodePalette8Block(ByteView block, std::size_t pixelCount, const std::vector<Rgba8Pixel>& paletteColors, BlockPixels& pixels)
{
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
        pixels[pixelIndex] = LookUpColor(paletteColors, block[pixelIndex]);
}

// Two bytes for each pixel, of which only the low 14 bits are the palette index.
void DecodePalette14Block(ByteView block, std::size_t pixelCount, const std::vector<Rgba8Pixel>& paletteColors, BlockPixels& pixels)
{
    ByteReader reader(block, "Texture block");
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
        pixels[pixelIndex] = LookUpColor(paletteColors, reader.ReadUint16() & PaletteIndexMask14Bits);
}

// How the colors in between are made from the two colors of a sub block: the weights of the first and of the second color.
// Thirds and the middle, as Yet Another Gamecube Documentation (17.14.1) gives them. Assumed: the division drops the rest (FMT-04).
struct MixWeights
{
    unsigned first;
    unsigned second;
};

constexpr MixWeights TwoThirdsFirst = {2, 1};
constexpr MixWeights TwoThirdsSecond = {1, 2};
constexpr MixWeights HalfAndHalf = {1, 1};

Byte MixChannel(Byte first, Byte second, MixWeights weights)
{
    return static_cast<Byte>((first * weights.first + second * weights.second) / (weights.first + weights.second));
}

Rgba8Pixel MixColors(const Rgba8Pixel& first, const Rgba8Pixel& second, MixWeights weights)
{
    Rgba8Pixel mixed;
    mixed.red = MixChannel(first.red, second.red, weights);
    mixed.green = MixChannel(first.green, second.green, weights);
    mixed.blue = MixChannel(first.blue, second.blue, weights);
    mixed.alpha = OpaqueAlpha;
    return mixed;
}

// A sub block stores two colors. If the first is the larger number, the other two colors are mixed from them
// in thirds. Otherwise the third color is the middle of the two and the fourth is transparent.
std::array<Rgba8Pixel, CompressedColorCount> ReadCompressedColors(ByteReader& reader)
{
    std::uint16_t firstColor = reader.ReadUint16();
    std::uint16_t secondColor = reader.ReadUint16();

    Rgba8Pixel first = DecodeRgb565Color(firstColor);
    Rgba8Pixel second = DecodeRgb565Color(secondColor);

    if (firstColor > secondColor)
        return {first, second, MixColors(first, second, TwoThirdsFirst), MixColors(first, second, TwoThirdsSecond)};

    return {first, second, MixColors(first, second, HalfAndHalf), Rgba8Pixel{0, 0, 0, 0}};
}

// A block is four sub blocks of 4 by 4 pixels: top left, top right, bottom left, bottom right.
// After its two colors a sub block has one byte for each row of pixels, with two bits for each pixel
// that pick one of the four colors. The leftmost pixel is in the high bits.
void DecodeCompressedBlock(ByteView block, BlockPixels& pixels)
{
    for (std::size_t subBlock = 0; subBlock < SubBlocksPerBlock; ++subBlock)
    {
        ByteReader reader(block.subspan(subBlock * SubBlockSize, SubBlockSize), "Texture block");
        std::array<Rgba8Pixel, CompressedColorCount> colors = ReadCompressedColors(reader);

        std::size_t left = (subBlock % SubBlocksPerRow) * SubBlockSide;
        std::size_t top = (subBlock / SubBlocksPerRow) * SubBlockSide;

        for (std::size_t row = 0; row < SubBlockSide; ++row)
        {
            Byte rowOfColorIndices = reader.ReadByte();

            for (std::size_t column = 0; column < SubBlockSide; ++column)
            {
                int lowestBit = static_cast<int>(BitsPerByte - BitsPerPixelIndex * (column + 1));
                std::size_t colorIndex = ExtractBits(rowOfColorIndices, lowestBit, BitsPerPixelIndex);
                std::size_t pixelIndex = (top + row) * CompressedBlockSide + left + column;
                pixels[pixelIndex] = colors[colorIndex];
            }
        }
    }
}

void DecodeBlock(TextureFormat format, ByteView block, std::size_t pixelCount, const std::vector<Rgba8Pixel>& paletteColors,
    BlockPixels& pixels)
{
    switch (format)
    {
    case TextureFormat::Intensity4:
        DecodeIntensity4Block(block, pixelCount, pixels);
        return;

    case TextureFormat::Intensity8:
        DecodeIntensity8Block(block, pixelCount, pixels);
        return;

    case TextureFormat::IntensityAlpha4:
        DecodeIntensityAlpha4Block(block, pixelCount, pixels);
        return;

    case TextureFormat::IntensityAlpha8:
        Decode16BitBlock(block, pixelCount, DecodeIntensityAlpha8Color, pixels);
        return;

    case TextureFormat::Rgb565:
        Decode16BitBlock(block, pixelCount, DecodeRgb565Color, pixels);
        return;

    case TextureFormat::Rgb5A3:
        Decode16BitBlock(block, pixelCount, DecodeRgb5A3Color, pixels);
        return;

    case TextureFormat::Rgba8:
        DecodeRgba8Block(block, pixelCount, pixels);
        return;

    case TextureFormat::Palette4:
        DecodePalette4Block(block, pixelCount, paletteColors, pixels);
        return;

    case TextureFormat::Palette8:
        DecodePalette8Block(block, pixelCount, paletteColors, pixels);
        return;

    case TextureFormat::Palette14:
        DecodePalette14Block(block, pixelCount, paletteColors, pixels);
        return;

    case TextureFormat::Compressed:
        DecodeCompressedBlock(block, pixels);
        return;
    }
}

// The blocks at the right and bottom edge of an image reach over it. The pixels outside the image are dropped.
void CopyBlockIntoImage(const BlockPixels& blockPixels, const BlockShape& shape, std::uint32_t blockColumn,
    std::uint32_t blockRow, RgbaImage& image)
{
    for (std::uint32_t row = 0; row < shape.height; ++row)
    {
        std::uint32_t y = blockRow * shape.height + row;
        if (y >= image.height)
            return;

        for (std::uint32_t column = 0; column < shape.width; ++column)
        {
            std::uint32_t x = blockColumn * shape.width + column;
            if (x >= image.width)
                break;

            image.pixels[std::size_t(y) * image.width + x] = blockPixels[row * shape.width + column];
        }
    }
}

}

RgbaImage DecodeTexture(TextureFormat format, std::uint32_t width, std::uint32_t height, ByteView encodedPixels,
    const std::optional<TexturePalette>& palette)
{
    std::size_t encodedSize = EncodedImageSize(format, width, height);
    if (encodedPixels.size() < encodedSize)
    {
        throw FormatError(std::format("Texture: an image of {} by {} pixels needs {} bytes, but only {} are given",
            width, height, encodedSize, encodedPixels.size()));
    }

    std::vector<Rgba8Pixel> paletteColors;
    if (IsPaletteFormat(format))
    {
        if (!palette)
            throw FormatError("Texture: a palette format needs a palette, but there is none");

        paletteColors = DecodePalette(*palette);
    }

    RgbaImage image;
    image.width = width;
    image.height = height;
    image.pixels.resize(std::size_t(width) * height);

    BlockShape shape = BlockShapeOf(format);
    std::size_t pixelCount = std::size_t(shape.width) * shape.height;
    std::uint32_t blocksAcross = static_cast<std::uint32_t>(DivideRoundingUp(width, shape.width));
    std::uint32_t blocksDown = static_cast<std::uint32_t>(DivideRoundingUp(height, shape.height));

    for (std::uint32_t blockRow = 0; blockRow < blocksDown; ++blockRow)
    {
        for (std::uint32_t blockColumn = 0; blockColumn < blocksAcross; ++blockColumn)
        {
            std::size_t blockNumber = std::size_t(blockRow) * blocksAcross + blockColumn;
            ByteView block = encodedPixels.subspan(blockNumber * shape.byteSize, shape.byteSize);

            BlockPixels blockPixels;
            DecodeBlock(format, block, pixelCount, paletteColors, blockPixels);
            CopyBlockIntoImage(blockPixels, shape, blockColumn, blockRow, image);
        }
    }

    return image;
}

}
