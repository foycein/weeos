#include "Formats/GifImage.h"

#include "Formats/FormatError.h"

#include <array>
#include <format>
#include <optional>
#include <string_view>
#include <vector>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "GIF picture";
constexpr std::string_view SignatureStart = "GIF";
constexpr std::size_t SignatureSize = 6;
constexpr std::size_t BackgroundAndAspectSize = 2;
constexpr std::size_t BytesPerColor = 3;
constexpr Byte OpaqueAlpha = 255;

// The blocks that follow the screen descriptor start with one of these bytes.
constexpr Byte ExtensionBlock = 0x21;
constexpr Byte ImageBlock = 0x2C;
constexpr Byte GraphicControlLabel = 0xF9;
constexpr std::size_t TransparentIndexPosition = 3;

constexpr Byte HasColorTableFlag = 0x80;
constexpr Byte ColorTableSizeMask = 0x07;
constexpr Byte InterlacedFlag = 0x40;
constexpr Byte HasTransparentColorFlag = 0x01;

constexpr Byte LowestMinimumCodeSize = 2;
constexpr int MaximumCodeSize = 12;
constexpr int CodeLimit = 1 << MaximumCodeSize;

// Reads the numbers of a GIF file, which start with the lowest byte.
class GifReader
{
public:
    explicit GifReader(ByteView bytesToRead)
        : bytes(bytesToRead)
    {
    }

    Byte ReadByte()
    {
        RequireBytes(1);
        Byte value = bytes[position];
        ++position;
        return value;
    }

    std::uint16_t ReadUint16()
    {
        Byte low = ReadByte();
        Byte high = ReadByte();
        return static_cast<std::uint16_t>(low | (high << BitsPerByte));
    }

    ByteView ReadBytes(std::size_t count)
    {
        RequireBytes(count);
        ByteView readBytes = bytes.subspan(position, count);
        position += count;
        return readBytes;
    }

    // Reads the pieces of data that each start with their size, up to the piece of size 0.
    Bytes ReadSubBlocks()
    {
        Bytes data;
        for (Byte size = ReadByte(); size != 0; size = ReadByte())
        {
            ByteView piece = ReadBytes(size);
            data.insert(data.end(), piece.begin(), piece.end());
        }

        return data;
    }

private:
    void RequireBytes(std::size_t count) const
    {
        if (position + count > bytes.size())
            throw FormatError(std::format("{}: the data ends too early", DataDescription));
    }

    ByteView bytes;
    std::size_t position = 0;
};

std::vector<Rgba8Pixel> ReadColorTable(GifReader& reader, Byte flags)
{
    // The flags hold the size as a power of two minus one.
    int sizeExponent = (flags & ColorTableSizeMask) + 1;
    std::size_t colorCount = std::size_t(1) << sizeExponent;
    ByteView colorBytes = reader.ReadBytes(colorCount * BytesPerColor);

    std::vector<Rgba8Pixel> palette;
    for (std::size_t color = 0; color < colorCount; ++color)
    {
        ByteView redGreenBlue = colorBytes.subspan(color * BytesPerColor, BytesPerColor);
        palette.push_back({redGreenBlue[0], redGreenBlue[1], redGreenBlue[2], OpaqueAlpha});
    }

    return palette;
}

// One entry of the table of the LZW compression: a string is the string of the entry before it plus one byte.
struct LzwEntry
{
    std::uint16_t previousEntry;
    Byte lastByte;
};

// Reads the codes of the compressed data, which are packed from the lowest bit of each byte up.
class CodeReader
{
public:
    explicit CodeReader(const Bytes& compressedBytes)
        : bytes(compressedBytes)
    {
    }

    // Gives back nothing when the data has no more bits for a code of this size.
    std::optional<int> ReadCode(int codeSize)
    {
        int code = 0;
        for (int bitNumber = 0; bitNumber < codeSize; ++bitNumber)
        {
            std::size_t byteNumber = bitPosition / BitsPerByte;
            if (byteNumber >= bytes.size())
                return std::nullopt;

            int bit = (bytes[byteNumber] >> (bitPosition % BitsPerByte)) & 1;
            code |= bit << bitNumber;
            ++bitPosition;
        }

        return code;
    }

private:
    const Bytes& bytes;
    std::size_t bitPosition = 0;
};

// The bytes of the string that the entry stands for. The table holds it backwards, from the last byte to the first.
Bytes ExpandEntry(const std::vector<LzwEntry>& table, int firstEntryOfStrings, int entry)
{
    Bytes reversedString;
    while (entry >= firstEntryOfStrings)
    {
        reversedString.push_back(table[entry].lastByte);
        entry = table[entry].previousEntry;
    }

    reversedString.push_back(static_cast<Byte>(entry));
    return Bytes(reversedString.rbegin(), reversedString.rend());
}

// Turns the compressed data into one palette index for each pixel.
Bytes DecodeLzw(Byte minimumCodeSize, const Bytes& compressedBytes, std::size_t pixelCount)
{
    int clearCode = 1 << minimumCodeSize;
    int endCode = clearCode + 1;
    int firstFreeEntry = endCode + 1;

    std::vector<LzwEntry> table(CodeLimit);
    int nextEntry = firstFreeEntry;
    int codeSize = minimumCodeSize + 1;
    std::optional<int> previousCode;

    CodeReader codes(compressedBytes);
    Bytes indexes;
    while (indexes.size() < pixelCount)
    {
        std::optional<int> code = codes.ReadCode(codeSize);
        if (!code || *code == endCode)
            break;

        if (*code == clearCode)
        {
            nextEntry = firstFreeEntry;
            codeSize = minimumCodeSize + 1;
            previousCode.reset();
            continue;
        }

        if (!previousCode)
        {
            if (*code >= clearCode)
                throw FormatError(std::format("{}: the first code after a clear code is not a color", DataDescription));

            indexes.push_back(static_cast<Byte>(*code));
            previousCode = *code;
            continue;
        }

        if (*code > nextEntry)
            throw FormatError(std::format("{}: a code that does not exist yet", DataDescription));

        // A code that is the next free entry stands for the string before it plus the first byte of that string.
        Bytes previousString = ExpandEntry(table, firstFreeEntry, *previousCode);
        Bytes codeString = previousString;
        if (*code == nextEntry)
            codeString.push_back(previousString.front());
        else
            codeString = ExpandEntry(table, firstFreeEntry, *code);

        indexes.insert(indexes.end(), codeString.begin(), codeString.end());

        if (nextEntry < CodeLimit)
        {
            table[nextEntry] = {static_cast<std::uint16_t>(*previousCode), codeString.front()};
            ++nextEntry;
        }

        bool needsBiggerCodes = nextEntry == (1 << codeSize) && codeSize < MaximumCodeSize;
        if (needsBiggerCodes)
            ++codeSize;

        previousCode = *code;
    }

    return indexes;
}

// The start of the file: the signature, the size and flags of the screen, and the colors that every picture can use.
std::vector<Rgba8Pixel> ReadScreenDescriptor(GifReader& reader)
{
    std::string signature(reinterpret_cast<const char*>(reader.ReadBytes(SignatureSize).data()), SignatureSize);
    if (!signature.starts_with(SignatureStart))
        throw FormatError(std::format("{}: the signature of a GIF picture is missing", DataDescription));

    reader.ReadUint16();
    reader.ReadUint16();
    Byte screenFlags = reader.ReadByte();
    reader.ReadBytes(BackgroundAndAspectSize);

    if ((screenFlags & HasColorTableFlag) == 0)
        return {};

    return ReadColorTable(reader, screenFlags);
}

// Goes through the blocks in front of the first picture. One of them may name the color that is transparent.
std::optional<Byte> SkipToImage(GifReader& reader)
{
    std::optional<Byte> transparentIndex;
    while (true)
    {
        Byte block = reader.ReadByte();
        if (block == ImageBlock)
            return transparentIndex;

        if (block != ExtensionBlock)
            throw FormatError(
                std::format("{}: the file has no picture, or a block of a kind that does not exist ({:#x})", DataDescription, block));

        Byte label = reader.ReadByte();
        Bytes extensionData = reader.ReadSubBlocks();
        bool isGraphicControl = label == GraphicControlLabel && extensionData.size() > TransparentIndexPosition;
        if (isGraphicControl && (extensionData[0] & HasTransparentColorFlag) != 0)
            transparentIndex = extensionData[TransparentIndexPosition];
    }
}

}

RgbaImage ReadGifImage(ByteView bytes)
{
    GifReader reader(bytes);
    std::vector<Rgba8Pixel> palette = ReadScreenDescriptor(reader);
    std::optional<Byte> transparentIndex = SkipToImage(reader);

    // The place of the picture on the screen is not needed: the picture is given with its own size.
    reader.ReadUint16();
    reader.ReadUint16();
    RgbaImage image;
    image.width = reader.ReadUint16();
    image.height = reader.ReadUint16();
    Byte imageFlags = reader.ReadByte();

    if ((imageFlags & InterlacedFlag) != 0)
        throw FormatError(std::format("{}: pictures with interlaced rows are not understood", DataDescription));

    if ((imageFlags & HasColorTableFlag) != 0)
        palette = ReadColorTable(reader, imageFlags);

    Byte minimumCodeSize = reader.ReadByte();
    if (minimumCodeSize < LowestMinimumCodeSize || minimumCodeSize >= MaximumCodeSize)
        throw FormatError(std::format("{}: the size {} of the first codes is not allowed", DataDescription, minimumCodeSize));

    std::size_t pixelCount = std::size_t(image.width) * image.height;
    Bytes indexes = DecodeLzw(minimumCodeSize, reader.ReadSubBlocks(), pixelCount);
    if (indexes.size() < pixelCount)
        throw FormatError(std::format("{}: the picture has {} pixels instead of {}", DataDescription, indexes.size(), pixelCount));

    indexes.resize(pixelCount);
    for (Byte index : indexes)
    {
        if (index >= palette.size())
            throw FormatError(std::format("{}: the color {} is not in the palette", DataDescription, index));

        Rgba8Pixel pixel = palette[index];
        if (transparentIndex && index == *transparentIndex)
            pixel.alpha = 0;

        image.pixels.push_back(pixel);
    }

    return image;
}

}
