#include "Text/TextLayouter.h"

#include "Text/TextError.h"

#include <algorithm>
#include <array>
#include <optional>

namespace Text
{

namespace
{

constexpr char16_t LineFeed = 0x000A;
constexpr char16_t Tab = 0x0009;

// The character that opens a mark and the character that closes it.
constexpr char16_t MarkCharacter = 0x001A;

// The characters before the space are control codes. They have no glyph and take no room; only the line feed (it ends the line)
// and the tab (it moves the pen) do something.
constexpr char16_t FirstPrintableCharacter = 0x0020;

// A tab moves the pen to the next multiple of four character widths. A character width is the horizontal size of the font.
// The text writer of the original starts with the count four (0x81519e94).
constexpr float TabStopCharacterCount = 4;

constexpr unsigned int FullAlpha = 255;

bool IsControlCode(char16_t character)
{
    return character < FirstPrintableCharacter;
}

// The control codes that are skipped: all but the tab. (The line feed ends the line before this is asked.)
bool IsSkippedControlCode(char16_t character)
{
    return IsControlCode(character) && character != Tab;
}

// The part of the free space that lies before the text: none for left and top, half for the middle, all of it for right and bottom.
float FreeSpaceBefore(HorizontalAlignment alignment)
{
    if (alignment == HorizontalAlignment::Left)
        return 0.0f;

    if (alignment == HorizontalAlignment::Center)
        return 0.5f;

    return 1.0f;
}

float FreeSpaceBefore(VerticalAlignment alignment)
{
    if (alignment == VerticalAlignment::Top)
        return 0.0f;

    if (alignment == VerticalAlignment::Middle)
        return 0.5f;

    return 1.0f;
}

Formats::Rgba8Color ScaleAlpha(Formats::Rgba8Color color, std::uint8_t paneAlpha)
{
    color.alpha = static_cast<Formats::Byte>(color.alpha * paneAlpha / FullAlpha);
    return color;
}

std::vector<CharacterRange> FindMarks(std::u16string_view text)
{
    std::vector<CharacterRange> marks;
    std::optional<std::size_t> openMarkStart;

    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] != MarkCharacter)
            continue;

        if (!openMarkStart)
        {
            openMarkStart = index + 1;
            continue;
        }

        marks.push_back({*openMarkStart, index});
        openMarkStart.reset();
    }

    if (openMarkStart)
        marks.push_back({*openMarkStart, text.size()});

    return marks;
}

// The characters of one line, without the line feed that ends it, and the width of the line.
struct Line
{
    std::size_t firstCharacter;
    std::size_t endCharacter;
    float width;
    bool endsWithLineFeed;
};

// A line and the place in the text where the next line starts.
struct TakenLine
{
    Line line;
    std::size_t nextLineStart;
    bool isLastLine;
};

// Where a line is: its number, where its pen starts, the left edge of its block and where its baseline lies.
struct LinePlace
{
    std::size_t lineNumber;
    float left;
    float blockLeft;
    float baselineY;
};

// The pen that moves along a line, and whether the character in front of it was a glyph (then the character space comes first).
struct Pen
{
    float x;
    bool followsGlyph;
};

// One run of the layout: it splits the text into lines, places the block of lines in the area and makes a quad for every character.
class LayoutJob
{
public:
    LayoutJob(const Formats::Font& fontToUse, const GlyphLookup& lookupToUse, const TextStyle& styleToUse, const TextArea& areaToFill,
        std::uint8_t paneAlpha, std::u16string_view textToLayOut)
        : font(fontToUse)
        , glyphLookup(lookupToUse)
        , style(styleToUse)
        , area(areaToFill)
        , text(textToLayOut)
        , topColor(ScaleAlpha(styleToUse.topColor, paneAlpha))
        , bottomColor(ScaleAlpha(styleToUse.bottomColor, paneAlpha))
    {
        scaleX = style.fontSize.x / font.width;
        scaleY = style.fontSize.y / font.height;
        lineHeight = style.lineSpace + font.leading * scaleY;
        fontHeight = font.height * scaleY;
    }

    LaidOutText Run() const
    {
        std::vector<Line> lines = SplitIntoLines();

        LaidOutText laidOutText;
        TextArea block = PlaceBlock(lines);
        laidOutText.block = block;
        laidOutText.blankColor = style.blankColor;
        laidOutText.inkColor = style.inkColor;
        for (std::size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
            AddLine(lines[lineNumber], lineNumber, block, laidOutText);

        laidOutText.marks = FindMarks(text);
        return laidOutText;
    }

private:
    // A text that ends with a line feed has an empty last line.
    std::vector<Line> SplitIntoLines() const
    {
        std::vector<Line> lines;
        std::size_t lineStart = 0;

        while (true)
        {
            TakenLine taken = TakeLine(lineStart);
            lines.push_back(taken.line);
            if (taken.isLastLine)
                return lines;

            lineStart = taken.nextLineStart;
        }
    }

    // Reads one line. It ends at a line feed, or before the character that would reach beyond the right edge of the area.
    // The first character of a line stays, even when it alone is wider than the area, and the line ends right behind it.
    TakenLine TakeLine(std::size_t lineStart) const
    {
        float lineWidth = 0;
        bool followsGlyph = false;

        for (std::size_t index = lineStart; index < text.size(); ++index)
        {
            char16_t character = text[index];
            if (character == LineFeed)
            {
                Line line = {.firstCharacter = lineStart, .endCharacter = index, .width = lineWidth, .endsWithLineFeed = true};
                return {.line = line, .nextLineStart = index + 1, .isLastLine = false};
            }

            if (IsSkippedControlCode(character))
                continue;

            float widthWithCharacter = WidthAfter(character, lineWidth, followsGlyph);
            bool reachesBeyondTheArea = widthWithCharacter > area.width;
            bool isFirstCharacterOfTheLine = index == lineStart;
            if (reachesBeyondTheArea && !isFirstCharacterOfTheLine)
            {
                Line line = {.firstCharacter = lineStart, .endCharacter = index, .width = lineWidth, .endsWithLineFeed = false};
                return {.line = line, .nextLineStart = index, .isLastLine = false};
            }

            lineWidth = widthWithCharacter;
            followsGlyph = character != Tab;
            if (reachesBeyondTheArea)
            {
                Line line = {.firstCharacter = lineStart, .endCharacter = index + 1, .width = lineWidth, .endsWithLineFeed = false};
                return {.line = line, .nextLineStart = index + 1, .isLastLine = false};
            }
        }

        Line line = {.firstCharacter = lineStart, .endCharacter = text.size(), .width = lineWidth, .endsWithLineFeed = false};
        return {.line = line, .nextLineStart = text.size(), .isLastLine = true};
    }

    // How far a line reaches after the character. A tab goes to the next tab stop. A glyph adds its advance, and the character space
    // when another glyph came before it.
    float WidthAfter(char16_t character, float widthBefore, bool followsGlyph) const
    {
        if (character == Tab)
            return NextTabStop(widthBefore);

        float characterSpace = followsGlyph ? style.characterSpace : 0.0f;
        return widthBefore + characterSpace + AdvanceOf(character);
    }

    // The distance from the origin of the tab stops to the first stop after the given distance.
    float NextTabStop(float distanceFromOrigin) const
    {
        float stopDistance = TabStopCharacterCount * style.fontSize.x;
        if (stopDistance <= 0)
            return distanceFromOrigin;

        int stopsBefore = static_cast<int>(distanceFromOrigin / stopDistance);
        return stopDistance * static_cast<float>(stopsBefore + 1);
    }

    // A text without line breaks is one line high. The menu measures a text with line breaks as one line height for each
    // break plus the height of the font for the last line.
    TextArea PlaceBlock(const std::vector<Line>& lines) const
    {
        float widestLineWidth = 0;
        for (const Line& line : lines)
            widestLineWidth = std::max(widestLineWidth, line.width);

        std::size_t lineBreakCount = lines.size() - 1;
        float blockHeight = lineHeight;
        if (lineBreakCount > 0)
            blockHeight = static_cast<float>(lineBreakCount) * lineHeight + fontHeight;

        float freeWidth = area.width - widestLineWidth;
        float freeHeight = area.height - blockHeight;

        TextArea block;
        block.left = area.left + freeWidth * FreeSpaceBefore(style.alignment.horizontal);
        block.top = area.top - freeHeight * FreeSpaceBefore(style.alignment.vertical);
        block.width = widestLineWidth;
        block.height = blockHeight;
        return block;
    }

    // The baseline of a line lies one ascent of the font below the top of the line.
    void AddLine(const Line& line, std::size_t lineNumber, const TextArea& block, LaidOutText& laidOutText) const
    {
        float freeWidth = block.width - line.width;
        float lineTop = block.top - static_cast<float>(lineNumber) * lineHeight;

        LinePlace place;
        place.lineNumber = lineNumber;
        place.left = block.left + freeWidth * FreeSpaceBefore(style.alignment.horizontal);
        place.blockLeft = block.left;
        place.baselineY = lineTop - font.ascent * scaleY;
        PlacedLine placedLine;
        placedLine.firstCharacter = line.firstCharacter;
        placedLine.endCharacter = line.endCharacter;
        placedLine.left = place.left;
        placedLine.width = line.width;
        placedLine.top = lineTop;
        placedLine.height = lineHeight;
        laidOutText.lines.push_back(placedLine);

        Pen pen = {place.left, false};
        for (std::size_t index = line.firstCharacter; index < line.endCharacter; ++index)
            AddCharacter(index, place, pen, laidOutText);

        if (line.endsWithLineFeed)
            laidOutText.characters.push_back({lineNumber, pen.x, 0});
    }

    // Every character of a line has a place. A control code takes no room, a tab takes the room up to its stop and a glyph its advance.
    void AddCharacter(std::size_t index, const LinePlace& place, Pen& pen, LaidOutText& laidOutText) const
    {
        char16_t character = text[index];
        PlacedCharacter placed = {place.lineNumber, pen.x, 0};

        if (character == Tab)
            MovePenToNextTabStop(place, pen, placed);
        else if (!IsControlCode(character))
            AddGlyph(index, place, pen, placed, laidOutText);

        laidOutText.characters.push_back(placed);
    }

    // No character space comes after a tab. The original measures a line with the stops counted from the start of the line, but it
    // prints with the stops counted from the left edge of the block (0x8151b558); the two are the same when the line starts at the edge.
    void MovePenToNextTabStop(const LinePlace& place, Pen& pen, PlacedCharacter& placed) const
    {
        float tabEnd = place.blockLeft + NextTabStop(pen.x - place.blockLeft);
        placed.advance = tabEnd - pen.x;
        pen.x = tabEnd;
        pen.followsGlyph = false;
    }

    // The character space comes first when a glyph stands in front of this one.
    void AddGlyph(std::size_t index, const LinePlace& place, Pen& pen, PlacedCharacter& placed, LaidOutText& laidOutText) const
    {
        char16_t character = text[index];
        if (pen.followsGlyph)
            pen.x += style.characterSpace;

        placed.left = pen.x;
        placed.advance = AdvanceOf(character);
        laidOutText.glyphs.push_back(MakeGlyph(character, index, pen.x, place.baselineY));

        pen.x += placed.advance;
        pen.followsGlyph = true;
    }

    // The quad starts the left spacing of the glyph after the pen and is as wide as the glyph. It reaches from the baseline position
    // of the font above the baseline down to the bottom of the cell. The texture coordinates cut out exactly that part of the sheet.
    PlacedGlyph MakeGlyph(char16_t character, std::size_t characterIndex, float penX, float baselineY) const
    {
        std::uint16_t glyphIndex = glyphLookup.FindGlyphIndex(character);
        Formats::GlyphWidths widths = font.WidthsOf(glyphIndex);
        Formats::GlyphCell cell = font.LocateGlyph(glyphIndex);

        float left = penX + widths.leftSpacing * scaleX;
        float right = left + widths.glyphWidth * scaleX;
        float top = baselineY + font.baselinePosition * scaleY;
        float bottom = top - cell.height * scaleY;

        float textureLeft = static_cast<float>(cell.left) / font.sheetWidth;
        float textureRight = static_cast<float>(cell.left + widths.glyphWidth) / font.sheetWidth;
        float textureTop = static_cast<float>(cell.top) / font.sheetHeight;
        float textureBottom = static_cast<float>(cell.top + cell.height) / font.sheetHeight;

        PlacedGlyph glyph{};
        glyph.sheetIndex = cell.sheetIndex;
        glyph.characterIndex = characterIndex;

        Graphics::Quad& quad = glyph.quad;
        quad.positions[LeftTopCorner] = {left, top};
        quad.positions[RightTopCorner] = {right, top};
        quad.positions[RightBottomCorner] = {right, bottom};
        quad.positions[LeftBottomCorner] = {left, bottom};

        quad.colors[LeftTopCorner] = topColor;
        quad.colors[RightTopCorner] = topColor;
        quad.colors[RightBottomCorner] = bottomColor;
        quad.colors[LeftBottomCorner] = bottomColor;

        std::array<Formats::Vector2, Graphics::QuadCornerCount>& coordinates = quad.textureCoordinates[0];
        coordinates[LeftTopCorner] = {textureLeft, textureTop};
        coordinates[RightTopCorner] = {textureRight, textureTop};
        coordinates[RightBottomCorner] = {textureRight, textureBottom};
        coordinates[LeftBottomCorner] = {textureLeft, textureBottom};
        return glyph;
    }

    // How far the pen moves after the character. The font stores the advance as a signed byte.
    float AdvanceOf(char16_t character) const
    {
        std::uint16_t glyphIndex = glyphLookup.FindGlyphIndex(character);
        std::int8_t advance = static_cast<std::int8_t>(font.WidthsOf(glyphIndex).characterWidth);
        return advance * scaleX;
    }

    const Formats::Font& font;
    const GlyphLookup& glyphLookup;
    const TextStyle& style;
    const TextArea& area;
    std::u16string_view text;

    // The size of the font units in layout units.
    float scaleX = 1;
    float scaleY = 1;

    // The distance from one line to the next, and the height of the characters.
    float lineHeight = 0;
    float fontHeight = 0;

    Formats::Rgba8Color topColor;
    Formats::Rgba8Color bottomColor;
};

}

TextLayouter::TextLayouter(const Formats::Font& fontToLayOut)
    : font(fontToLayOut)
    , glyphLookup(fontToLayOut)
{
    if (font.width == 0 || font.height == 0)
        throw TextError("the font has no size");
}

LaidOutText TextLayouter::LayOut(std::u16string_view text, const TextStyle& style, const TextArea& area, std::uint8_t paneAlpha) const
{
    LayoutJob job(font, glyphLookup, style, area, paneAlpha, text);
    return job.Run();
}

}
