#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/TextureFormat.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Formats
{

enum class FontEncoding : std::uint8_t
{
    Utf8 = 0,
    Utf16 = 1,
    ShiftJis = 2,
    Cp1252 = 3,
};

// How far a glyph is drawn from the position of the pen, how wide its picture is and how far the pen moves on.
struct GlyphWidths
{
    std::int8_t leftSpacing;
    std::uint8_t glyphWidth;
    std::uint8_t characterWidth;
};

// The widths of the glyphs from the first to the last glyph index.
struct GlyphWidthRange
{
    std::uint16_t firstGlyphIndex;
    std::uint16_t lastGlyphIndex;
    std::vector<GlyphWidths> widths;
};

enum class CharacterMapMethod : std::uint16_t
{
    // The glyph indices go up by one from the first character to the last.
    Direct = 0,

    // A table gives the glyph index of every character from the first to the last.
    Table = 1,

    // A list of pairs of a character and a glyph index.
    Scan = 2,
};

struct CharacterMapPair
{
    std::uint16_t characterCode;
    std::uint16_t glyphIndex;
};

// Tells which glyph a character has. A font has several maps, each for a range of characters.
struct CharacterMap
{
    std::uint16_t firstCharacterCode;
    std::uint16_t lastCharacterCode;
    CharacterMapMethod method;

    // Only for the method Direct.
    std::uint16_t firstGlyphIndex;

    // Only for the method Table. The value 0xFFFF means that the character has no glyph.
    std::vector<std::uint16_t> glyphIndices;

    // Only for the method Scan.
    std::vector<CharacterMapPair> pairs;
};

// Where the picture of a glyph is on the glyph sheets.
struct GlyphCell
{
    std::size_t sheetIndex;
    std::uint32_t left;
    std::uint32_t top;
    std::uint32_t width;
    std::uint32_t height;
};

// A font (BRFNT file, or BRFNA file for the font archives of the web browser) with the pictures of its glyphs.
// The pictures are in sheets, and a sheet is a grid of cells that each hold one glyph.
struct Font
{
    std::uint8_t fontType;
    std::uint8_t leading;
    std::uint16_t defaultGlyphIndex;
    GlyphWidths defaultWidths;
    FontEncoding encoding;
    std::uint8_t height;
    std::uint8_t width;
    std::uint8_t ascent;

    std::uint8_t cellWidth;
    std::uint8_t cellHeight;
    std::uint8_t baselinePosition;
    std::uint8_t maximumCharacterWidth;
    TextureFormat sheetFormat;
    std::uint16_t cellsPerRow;
    std::uint16_t cellsPerColumn;
    std::uint16_t sheetWidth;
    std::uint16_t sheetHeight;

    // The stored pixels of each sheet, in the layout of the sheet format. Compressed sheets are decompressed.
    std::vector<Bytes> sheets;

    std::vector<GlyphWidthRange> widthRanges;
    std::vector<CharacterMap> characterMaps;

    // The glyph index of a character, or nothing when the font has no glyph for it.
    std::optional<std::uint16_t> FindGlyphIndex(std::uint16_t characterCode) const;

    // The widths of a glyph. A glyph that the font gives no widths for has the default widths.
    GlyphWidths WidthsOf(std::uint16_t glyphIndex) const;

    // The place of a glyph on the sheets. There is a gap of one pixel between the cells of a sheet.
    GlyphCell LocateGlyph(std::uint16_t glyphIndex) const;

    RgbaImage DecodeSheet(std::size_t sheetIndex) const;
};

Font ReadFont(ByteView bytes);

}
