#include "Formats/PngImage.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/Inflate.h"

#include <array>
#include <cstdlib>
#include <format>
#include <string>

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

constexpr Byte EightBitsPerColor = 8;
constexpr Byte RedGreenBlueAlphaColors = 6;
constexpr Byte DeflateCompression = 0;
constexpr Byte AdaptiveFiltering = 0;
constexpr Byte NotInterlaced = 0;

constexpr std::size_t BytesPerPixel = 4;

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
};

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

    bool isUnderstood = bitsPerColor == EightBitsPerColor && colorKind == RedGreenBlueAlphaColors && compression == DeflateCompression
        && filtering == AdaptiveFiltering && interlacing == NotInterlaced;
    if (!isUnderstood)
        chunkReader.Fail("only pictures with 8 bits of red, green, blue and alpha, in rows one after the other, are understood");

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
    std::size_t rowSize = header.width * BytesPerPixel;
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
            Byte left = column >= BytesPerPixel ? pixels[row * rowSize + column - BytesPerPixel] : 0;
            Byte above = row > 0 ? pixels[(row - 1) * rowSize + column] : 0;
            Byte aboveLeft = (row > 0 && column >= BytesPerPixel) ? pixels[(row - 1) * rowSize + column - BytesPerPixel] : 0;
            pixels[row * rowSize + column] = UnfilterByte(filter, filtered, left, above, aboveLeft);
        }
    }

    return pixels;
}

RgbaImage MakeImage(const Header& header, const Bytes& pixelBytes)
{
    RgbaImage image;
    image.width = header.width;
    image.height = header.height;

    for (std::size_t pixelNumber = 0; pixelNumber < pixelBytes.size() / BytesPerPixel; ++pixelNumber)
    {
        const Byte* pixelByte = &pixelBytes[pixelNumber * BytesPerPixel];
        image.pixels.push_back({pixelByte[0], pixelByte[1], pixelByte[2], pixelByte[3]});
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
        else if (chunkType == EndChunk)
        {
            break;
        }
    }

    if (!hasHeader)
        reader.Fail("the header chunk is missing");

    return MakeImage(header, Unfilter(InflateZlib(compressedRows), header));
}

}
