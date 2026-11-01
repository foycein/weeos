#include "Formats/Font.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/Huffman.h"
#include "Formats/SectionedFile.h"
#include "Formats/TextureDecoder.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "Font";
constexpr std::size_t TagSize = 4;
constexpr std::uint16_t SupportedVersion = 0x0104;
constexpr std::uint16_t CompressedSheetsFlag = 0x8000;
constexpr std::uint16_t NoGlyph = 0xFFFF;
constexpr std::uint32_t CellGap = 1;

// The font information holds the offsets of the other sections, which follow in the file anyway.
constexpr std::size_t SectionOffsetsSize = 12;
constexpr std::size_t UnusedAfterCharacterMapMethodSize = 2;
constexpr std::size_t NextSectionOffsetSize = 4;

// The first section of a font archive, a table of the glyph groups. The pictures and maps of the fonts do not need it.
constexpr std::string_view GlyphGroupTableTag = "GLGR";

void ReadFontInformation(const Section& section, Font& font)
{
    ByteReader reader = OpenSection(section, DataDescription);

    font.fontType = reader.ReadByte();
    font.leading = reader.ReadByte();
    font.defaultGlyphIndex = reader.ReadUint16();
    font.defaultWidths.leftSpacing = static_cast<std::int8_t>(reader.ReadByte());
    font.defaultWidths.glyphWidth = reader.ReadByte();
    font.defaultWidths.characterWidth = reader.ReadByte();

    Byte encoding = reader.ReadByte();
    if (encoding > static_cast<Byte>(FontEncoding::Cp1252))
        reader.Fail(std::format("the encoding {} is not known", encoding));
    font.encoding = static_cast<FontEncoding>(encoding);

    reader.Skip(SectionOffsetsSize);
    font.height = reader.ReadByte();
    font.width = reader.ReadByte();
    font.ascent = reader.ReadByte();
}

Bytes ReadCompressedSheet(ByteReader& reader)
{
    std::uint32_t compressedSize = reader.ReadUint32();
    return DecompressHuffman(reader.ReadBytes(compressedSize));
}

Bytes ReadRawSheet(ByteReader& reader, std::uint32_t sheetSize)
{
    ByteView sheet = reader.ReadBytes(sheetSize);
    return Bytes(sheet.begin(), sheet.end());
}

void ReadGlyphSheets(const Section& section, ByteView fileBytes, Font& font)
{
    ByteReader reader = OpenSection(section, DataDescription);

    font.cellWidth = reader.ReadByte();
    font.cellHeight = reader.ReadByte();
    font.baselinePosition = reader.ReadByte();
    font.maximumCharacterWidth = reader.ReadByte();
    std::uint32_t sheetSize = reader.ReadUint32();
    std::uint16_t sheetCount = reader.ReadUint16();

    std::uint16_t formatAndFlag = reader.ReadUint16();
    bool sheetsAreCompressed = (formatAndFlag & CompressedSheetsFlag) != 0;
    std::uint32_t formatNumber = formatAndFlag & ~static_cast<std::uint32_t>(CompressedSheetsFlag);
    std::optional<TextureFormat> format = TextureFormatFromNumber(formatNumber);
    if (!format)
        reader.Fail(std::format("the sheet format {} is not known", formatNumber));
    font.sheetFormat = *format;

    font.cellsPerRow = reader.ReadUint16();
    font.cellsPerColumn = reader.ReadUint16();
    font.sheetWidth = reader.ReadUint16();
    font.sheetHeight = reader.ReadUint16();
    std::uint32_t sheetsOffset = reader.ReadUint32();

    if (font.cellsPerRow == 0 || font.cellsPerColumn == 0)
        reader.Fail("the sheets have no cells");

    if (EncodedImageSize(font.sheetFormat, font.sheetWidth, font.sheetHeight) > sheetSize)
        reader.Fail(std::format("a sheet of {} by {} pixels does not fit in {} bytes", font.sheetWidth, font.sheetHeight, sheetSize));

    ByteReader sheetReader(fileBytes, DataDescription);
    sheetReader.MoveTo(sheetsOffset);
    for (std::uint16_t sheetNumber = 0; sheetNumber < sheetCount; ++sheetNumber)
    {
        Bytes sheet = sheetsAreCompressed ? ReadCompressedSheet(sheetReader) : ReadRawSheet(sheetReader, sheetSize);
        if (sheet.size() != sheetSize)
            sheetReader.Fail(std::format("the sheet {} has {} bytes instead of {}", sheetNumber, sheet.size(), sheetSize));

        font.sheets.push_back(std::move(sheet));
    }
}

GlyphWidthRange ReadWidthRange(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);

    GlyphWidthRange range;
    range.firstGlyphIndex = reader.ReadUint16();
    range.lastGlyphIndex = reader.ReadUint16();
    reader.Skip(NextSectionOffsetSize);
    if (range.lastGlyphIndex < range.firstGlyphIndex)
        reader.Fail(std::format("the last glyph {} comes before the first glyph {}", range.lastGlyphIndex, range.firstGlyphIndex));

    for (std::uint32_t glyphIndex = range.firstGlyphIndex; glyphIndex <= range.lastGlyphIndex; ++glyphIndex)
    {
        GlyphWidths widths;
        widths.leftSpacing = static_cast<std::int8_t>(reader.ReadByte());
        widths.glyphWidth = reader.ReadByte();
        widths.characterWidth = reader.ReadByte();
        range.widths.push_back(widths);
    }
    return range;
}

CharacterMapMethod ReadMapMethod(ByteReader& reader)
{
    std::uint16_t number = reader.ReadUint16();
    if (number > static_cast<std::uint16_t>(CharacterMapMethod::Scan))
        reader.Fail(std::format("the mapping method {} is not known", number));

    return static_cast<CharacterMapMethod>(number);
}

CharacterMap ReadCharacterMap(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);

    CharacterMap map;
    map.firstCharacterCode = reader.ReadUint16();
    map.lastCharacterCode = reader.ReadUint16();
    map.method = ReadMapMethod(reader);
    reader.Skip(UnusedAfterCharacterMapMethodSize);
    reader.Skip(NextSectionOffsetSize);
    map.firstGlyphIndex = 0;

    if (map.method == CharacterMapMethod::Direct)
    {
        map.firstGlyphIndex = reader.ReadUint16();
    }
    else if (map.method == CharacterMapMethod::Table)
    {
        for (std::uint32_t code = map.firstCharacterCode; code <= map.lastCharacterCode; ++code)
            map.glyphIndices.push_back(reader.ReadUint16());
    }
    else
    {
        std::uint16_t pairCount = reader.ReadUint16();
        for (std::uint16_t pairNumber = 0; pairNumber < pairCount; ++pairNumber)
        {
            CharacterMapPair pair;
            pair.characterCode = reader.ReadUint16();
            pair.glyphIndex = reader.ReadUint16();
            map.pairs.push_back(pair);
        }
    }

    return map;
}

// A font archive tells its tag RFNA and a font tells RFNT. Their sections are the same, except that
// an archive has the glyph group table in front.
std::string_view FindFontTag(ByteView bytes)
{
    ByteReader reader(bytes, DataDescription);
    std::string tag = reader.ReadText(TagSize);
    return tag == "RFNA" ? "RFNA" : "RFNT";
}

}

Font ReadFont(ByteView bytes)
{
    SectionedFile file = ReadSectionedFile(bytes, FindFontTag(bytes), DataDescription);
    if (file.version != SupportedVersion)
    {
        throw FormatError(std::format("{}: the version {:#x} is not supported, only version {:#x} is known", DataDescription,
            file.version, SupportedVersion));
    }

    SectionCursor cursor(file.sections, DataDescription);
    if (cursor.NextIs(GlyphGroupTableTag))
        cursor.Take();

    Font font;
    ReadFontInformation(cursor.Take("FINF"), font);
    ReadGlyphSheets(cursor.Take("TGLP"), bytes, font);

    while (cursor.NextIs("CWDH"))
        font.widthRanges.push_back(ReadWidthRange(cursor.Take()));

    while (cursor.NextIs("CMAP"))
        font.characterMaps.push_back(ReadCharacterMap(cursor.Take()));

    if (!cursor.IsAtEnd())
        throw FormatError(std::format("{}: the section {} comes at a place where no section is expected", DataDescription, cursor.Take().tag));

    return font;
}

std::optional<std::uint16_t> Font::FindGlyphIndex(std::uint16_t characterCode) const
{
    for (const CharacterMap& map : characterMaps)
    {
        bool isInMap = characterCode >= map.firstCharacterCode && characterCode <= map.lastCharacterCode;
        if (!isInMap)
            continue;

        if (map.method == CharacterMapMethod::Direct)
            return static_cast<std::uint16_t>(map.firstGlyphIndex + characterCode - map.firstCharacterCode);

        if (map.method == CharacterMapMethod::Table)
        {
            std::uint16_t glyphIndex = map.glyphIndices[characterCode - map.firstCharacterCode];
            if (glyphIndex != NoGlyph)
                return glyphIndex;
            continue;
        }

        for (const CharacterMapPair& pair : map.pairs)
        {
            if (pair.characterCode == characterCode)
                return pair.glyphIndex;
        }
    }

    return std::nullopt;
}

GlyphWidths Font::WidthsOf(std::uint16_t glyphIndex) const
{
    for (const GlyphWidthRange& range : widthRanges)
    {
        if (glyphIndex >= range.firstGlyphIndex && glyphIndex <= range.lastGlyphIndex)
            return range.widths[glyphIndex - range.firstGlyphIndex];
    }

    return defaultWidths;
}

GlyphCell Font::LocateGlyph(std::uint16_t glyphIndex) const
{
    std::size_t glyphsPerSheet = std::size_t(cellsPerRow) * cellsPerColumn;
    std::size_t indexOnSheet = glyphIndex % glyphsPerSheet;

    std::uint32_t column = static_cast<std::uint32_t>(indexOnSheet % cellsPerRow);
    std::uint32_t row = static_cast<std::uint32_t>(indexOnSheet / cellsPerRow);

    GlyphCell cell;
    cell.sheetIndex = glyphIndex / glyphsPerSheet;
    cell.left = column * (cellWidth + CellGap) + CellGap;
    cell.top = row * (cellHeight + CellGap) + CellGap;
    cell.width = cellWidth;
    cell.height = cellHeight;
    return cell;
}

RgbaImage Font::DecodeSheet(std::size_t sheetIndex) const
{
    return DecodeTexture(sheetFormat, sheetWidth, sheetHeight, sheets.at(sheetIndex), std::nullopt);
}

}
