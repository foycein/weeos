#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Text/TextStyle.h"

#include <cstddef>
#include <vector>

namespace Text
{

// The order of the corners of a glyph quad, as the graphics device wants it.
constexpr std::size_t LeftTopCorner = 0;
constexpr std::size_t RightTopCorner = 1;
constexpr std::size_t RightBottomCorner = 2;
constexpr std::size_t LeftBottomCorner = 3;

// One glyph of a text: a quad that shows a part of one glyph sheet of the font.
struct PlacedGlyph
{
    std::size_t sheetIndex;

    // The place in the text of the character that the glyph shows. A program that colors or moves a part of the text finds the glyphs of it with this.
    std::size_t characterIndex;

    // The positions are in the layout units of the text area. The colors are the colors of the style, with the pane alpha applied.
    // The texture coordinates are in set 0.
    Graphics::Quad quad;
};

// One line of a laid-out text. Its characters are those from firstCharacter up to, but not including, endCharacter.
// A line feed that ends the line is not one of them (it has the index endCharacter). A line that was broken at the edge of the area has no line feed.
struct PlacedLine
{
    std::size_t firstCharacter;
    std::size_t endCharacter;

    // Where the pen of the line starts, how far the line reaches, and the upper edge and the height of the line.
    float left;
    float width;
    float top;
    float height;
};

// Where a character of the text is. Every character of the text has one, also the line feeds and the other control codes.
struct PlacedCharacter
{
    std::size_t lineNumber;

    // The pen position in front of the character and how far the pen moves on. A control code moves it by nothing,
    // a tab by the room up to the next tab stop. A line feed is at the end of the line that it ends.
    float left;
    float advance;
};

// The characters from first up to, but not including, end.
struct CharacterRange
{
    std::size_t first;
    std::size_t end;
};

struct LaidOutText
{
    std::vector<PlacedGlyph> glyphs;

    // The place of all lines together inside the text area.
    TextArea block;

    // The colors that the glyphs are drawn between, as the style says.
    Formats::Rgba8Color blankColor;
    Formats::Rgba8Color inkColor;

    // The lines, and one entry for every character of the text, in the order of the text.
    std::vector<PlacedLine> lines;
    std::vector<PlacedCharacter> characters;

    // The ranges that the text marks with the character U+001A: it opens a mark and the next U+001A closes it.
    // The marked characters are those between the two. A mark that is not closed reaches to the end of the text.
    // The Message Board colors marks and lets the pointer point at them; the marks need nothing else from the text engine.
    std::vector<CharacterRange> marks;
};

// The rectangles that cover the characters from first up to, but not including, end: one for every line that the range touches,
// from the start of its first character on that line to the end of its last character. A line in the middle of the range is covered
// completely. An empty range (first equals end) gives one rectangle without width where the pen is in front of the character,
// which is where a text cursor stands. The end of the text is a possible place for it.
// Throws a TextError when the range reaches beyond the text.
std::vector<TextArea> AreasOfCharacters(const LaidOutText& text, std::size_t first, std::size_t end);

}
