#include "Formats/PngImage.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/Inflate.h"

#include <array>
#include <cstdlib>
#include <format>
#include <string>
#include <vector>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "PNG picture";
constexpr std::array<Byte, 8> Signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

constexpr std::size_t ChunkTypeSize = 4;
constexpr std::size_t ChunkCheckNumberSize = 4;
constexpr std::string_view HeaderChunk = "IHDR";
constexpr std::string_view DataChunk = "IDAT";
constexpr std::string_view EndChunk = "IEND";
constexpr std::string_view PaletteChunk = "PLTE";
constexpr std::string_view TransparencyChunk = "tRNS";

constexpr Byte EightBitsPerColor = 8;
constexpr Byte RedGreenBlueAlphaColors = 6;
constexpr Byte RedGreenBlueColors = 2;
constexpr Byte PaletteColors = 3;
constexpr Byte DeflateCompression = 0;
constexpr Byte AdaptiveFiltering = 0;
constexpr Byte NotInterlaced = 0;

constexpr std::size_t BytesPerPixelWithAlpha = 4;
constexpr std::size_t BytesPerPixelWithoutAlpha = 3;
constexpr std::size_t BytesPerPaletteIndex = 1;
constexpr std::size_t BytesPerPaletteColor = 3;
constexpr Byte OpaqueAlpha = 255;

// The first byte of every row says how the row was filtered before it was compressed.
constexpr Byte NoFilter = 0;
constexpr Byte SubtractLeftFilter = 1;
constexpr Byte SubtractAboveFilter = 2;
constexpr Byte SubtractAverageFilter = 3;
constexpr Byte PaethFilter = 4;

struct Header
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // 4 with alpha, 3 for a picture without (the start page of the Internet Channel has one), 1 for a picture whose pixels are the
    // numbers of colors of its palette (the manuals of the channels have them).
    std::size_t bytesPerPixel = BytesPerPixelWithAlpha;
    bool hasPalette = false;
};

// The colors of a picture with a palette (PLTE), with the alpha of the first ones from tRNS; the others are opaque.
using Palette = std::vector<Rgba8Color>;

Header ReadHeader(ByteReader& chunkReader)
{
    Header header;
    header.width = chunkReader.ReadUint32();
    header.height = chunkReader.ReadUint32();

    Byte bitsPerColor = chunkReader.ReadByte();
    Byte colorKind = chunkReader.ReadByte();
    Byte compression = chunkReader.ReadByte();
    Byte filtering = chunkReader.ReadByte();
    Byte interlacing = chunkReader.ReadByte();

    bool hasKnownColors = colorKind == RedGreenBlueAlphaColors || colorKind == RedGreenBlueColors || colorKind == PaletteColors;
    bool isUnderstood = bitsPerColor == EightBitsPerColor && hasKnownColors && compression == DeflateCompression
        && filtering == AdaptiveFiltering && interlacing == NotInterlaced;
    if (!isUnderstood)
        chunkReader.Fail("only pictures with 8 bits of red, green, blue and maybe alpha or of a palette, in rows one after the other, are understood");

    if (colorKind == RedGreenBlueColors)
        header.bytesPerPixel = BytesPerPixelWithoutAlpha;

    if (colorKind == PaletteColors)
    {
        header.bytesPerPixel = BytesPerPaletteIndex;
        header.hasPalette = true;
    }

    return header;
}

// The Paeth filter predicts a byte from three neighbours: the byte on the left, the one above, and the one above on the left.
// The neighbour that lies nearest to left + above - aboveLeft is the prediction.
Byte PaethPredictor(Byte left, Byte above, Byte aboveLeft)
{
    int estimate = left + above - aboveLeft;
    int distanceToLeft = std::abs(estimate - left);
    int distanceToAbove = std::abs(estimate - above);
    int distanceToAboveLeft = std::abs(estimate - aboveLeft);

    if (distanceToLeft <= distanceToAbove && distanceToLeft <= distanceToAboveLeft)
        return left;

    if (distanceToAbove <= distanceToAboveLeft)
        return above;

    return aboveLeft;
}

Byte UnfilterByte(Byte filter, Byte filtered, Byte left, Byte above, Byte aboveLeft)
{
    switch (filter)
    {
    case NoFilter:
        return filtered;

    case SubtractLeftFilter:
        return static_cast<Byte>(filtered + left);

    case SubtractAboveFilter:
        return static_cast<Byte>(filtered + above);

    case SubtractAverageFilter:
        return static_cast<Byte>(filtered + (left + above) / 2);

    case PaethFilter:
        return static_cast<Byte>(filtered + PaethPredictor(left, above, aboveLeft));
    }

    throw FormatError(std::format("{}: the filter {} does not exist", DataDescription, filter));
}

// Turns the rows as they were compressed (a filter byte, then the filtered bytes of the row) into the bytes of the pixels.
Bytes Unfilter(ByteView filteredRows, const Header& header)
{
    std::size_t bytesPerPixel = header.bytesPerPixel;
    std::size_t rowSize = header.width * bytesPerPixel;
    std::size_t filteredRowSize = 1 + rowSize;
    if (filteredRows.size() != filteredRowSize * header.height)
        throw FormatError(std::format("{}: the picture has {} bytes instead of {}", DataDescription, filteredRows.size(), filteredRowSize * header.height));

    Bytes pixels(rowSize * header.height);
    for (std::size_t row = 0; row < header.height; ++row)
    {
        Byte filter = filteredRows[row * filteredRowSize];
        for (std::size_t column = 0; column < rowSize; ++column)
        {
            Byte filtered = filteredRows[row * filteredRowSize + 1 + column];
            Byte left = column >= bytesPerPixel ? pixels[row * rowSize + column - bytesPerPixel] : 0;
            Byte above = row > 0 ? pixels[(row - 1) * rowSize + column] : 0;
            Byte aboveLeft = (row > 0 && column >= bytesPerPixel) ? pixels[(row - 1) * rowSize + column - bytesPerPixel] : 0;
            pixels[row * rowSize + column] = UnfilterByte(filter, filtered, left, above, aboveLeft);
        }
    }

    return pixels;
}

Palette ReadPalette(ByteReader& chunkReader)
{
    Palette palette;
    while (chunkReader.RemainingSize() >= BytesPerPaletteColor)
    {
        Byte red = chunkReader.ReadByte();
        Byte green = chunkReader.ReadByte();
        Byte blue = chunkReader.ReadByte();
        palette.push_back({red, green, blue, OpaqueAlpha});
    }
    return palette;
}

void ReadPaletteAlpha(ByteReader& chunkReader, Palette& palette)
{
    for (Rgba8Color& color : palette)
    {
        if (chunkReader.IsAtEnd())
            return;

        color.alpha = chunkReader.ReadByte();
    }
}

RgbaImage MakeImage(const Header& header, const Bytes& pixelBytes, const Palette& palette)
{
    RgbaImage image;
    image.width = header.width;
    image.height = header.height;

    if (header.hasPalette)
    {
        for (Byte colorNumber : pixelBytes)
        {
            if (colorNumber >= palette.size())
                throw FormatError(std::format("{}: the color {} is not in the palette", DataDescription, colorNumber));

            image.pixels.push_back(palette[colorNumber]);
        }
        return image;
    }

    std::size_t bytesPerPixel = header.bytesPerPixel;
    for (std::size_t pixelNumber = 0; pixelNumber < pixelBytes.size() / bytesPerPixel; ++pixelNumber)
    {
        const Byte* pixelByte = &pixelBytes[pixelNumber * bytesPerPixel];
        Byte alpha = bytesPerPixel == BytesPerPixelWithAlpha ? pixelByte[3] : OpaqueAlpha;
        image.pixels.push_back({pixelByte[0], pixelByte[1], pixelByte[2], alpha});
    }

    return image;
}

}

RgbaImage ReadPngImage(ByteView bytes)
{
    ByteReader reader(bytes, DataDescription);
    if (reader.ReadByteArray<Signature.size()>() != Signature)
        reader.Fail("the signature of a PNG picture is missing");

    Header header;
    bool hasHeader = false;
    Bytes compressedRows;
    Palette palette;

    while (true)
    {
        std::uint32_t chunkSize = reader.ReadUint32();
        std::string chunkType = reader.ReadText(ChunkTypeSize);
        ByteReader chunkReader(reader.ReadBytes(chunkSize), DataDescription);
        reader.Skip(ChunkCheckNumberSize);

        if (chunkType == HeaderChunk)
        {
            header = ReadHeader(chunkReader);
            hasHeader = true;
        }
        else if (chunkType == DataChunk)
        {
            ByteView data = chunkReader.ReadBytes(chunkSize);
            compressedRows.insert(compressedRows.end(), data.begin(), data.end());
        }
        else if (chunkType == PaletteChunk)
        {
            palette = ReadPalette(chunkReader);
        }
        else if (chunkType == TransparencyChunk && header.hasPalette)
        {
            ReadPaletteAlpha(chunkReader, palette);
        }
        else if (chunkType == EndChunk)
        {
            break;
        }
    }

    if (!hasHeader)
        reader.Fail("the header chunk is missing");

    return MakeImage(header, Unfilter(InflateZlib(compressedRows), header), palette);
}

}
