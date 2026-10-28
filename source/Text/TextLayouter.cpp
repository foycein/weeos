#include "Text/TextLayouter.h"

#include "Text/TextError.h"

#include <algorithm>

namespace Text
{

namespace
{

constexpr char16_t LineFeed = 0x000A;

// The characters before the space are control codes. They have no glyph and take no room.
constexpr char16_t FirstPrintableCharacter = 0x0020;

constexpr unsigned int FullAlpha = 255;

bool IsControlCode(char16_t character)
{
    return character < FirstPrintableCharacter;
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

// The characters of one line without the line feed that ends it, and the width of the line.
struct Line
{
    std::u16string_view characters;
    float width;
};

// One run of the layout: it splits the text into lines, places the block of lines in the area and makes a quad for every character.
class LayoutJob
{
public:
    LayoutJob(const Formats::Font& fontToUse, const GlyphLookup& lookupToUse, const TextStyle& styleToUse, const TextArea& areaToFill,
        std::uint8_t paneAlpha)
        : font(fontToUse)
        , glyphLookup(lookupToUse)
        , style(styleToUse)
        , area(areaToFill)
        , topColor(ScaleAlpha(styleToUse.topColor, paneAlpha))
        , bottomColor(ScaleAlpha(styleToUse.bottomColor, paneAlpha))
    {
        scaleX = style.fontSize.x / font.width;
        scaleY = style.fontSize.y / font.height;
        lineHeight = style.lineSpace + font.leading * scaleY;
        fontHeight = font.height * scaleY;
    }

    LaidOutText Run(std::u16string_view text) const
    {
        std::vector<Line> lines = SplitIntoLines(text);

        LaidOutText laidOutText;
        TextArea block = PlaceBlock(lines);
        laidOutText.block = block;
        for (std::size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
            AddGlyphsOfLine(lines[lineNumber], lineNumber, block, laidOutText);

        return laidOutText;
    }

private:
    // How far the pen moves after the character.
    float AdvanceOf(char16_t character) const
    {
        std::uint16_t glyphIndex = glyphLookup.FindGlyphIndex(character);
        return font.WidthsOf(glyphIndex).characterWidth * scaleX;
    }

    // A line and the place in the text where the next line starts.
    struct TakenLine
    {
        Line line;
        std::size_t nextLineStart;
        bool isLastLine;
    };

    // Reads one line. It ends at a line feed, or before the character that would reach beyond the right edge of the area.
    // A line always keeps its first character, even when that one alone is wider than the area.
    TakenLine TakeLine(std::u16string_view text, std::size_t lineStart) const
    {
        float lineWidth = 0;
        bool lineHasGlyph = false;

        for (std::size_t index = lineStart; index < text.size(); ++index)
        {
            char16_t character = text[index];
            if (character == LineFeed)
                return {{text.substr(lineStart, index - lineStart), lineWidth}, index + 1, false};

            if (IsControlCode(character))
                continue;

            float widthWithCharacter = lineWidth + AdvanceOf(character);
            if (lineHasGlyph)
                widthWithCharacter += style.characterSpace;

            bool reachesBeyondTheArea = widthWithCharacter > area.width;
            if (lineHasGlyph && reachesBeyondTheArea)
                return {{text.substr(lineStart, index - lineStart), lineWidth}, index, false};

            lineWidth = widthWithCharacter;
            lineHasGlyph = true;
        }

        return {{text.substr(lineStart), lineWidth}, text.size(), true};
    }

    // A text that ends with a line feed has an empty last line.
    std::vector<Line> SplitIntoLines(std::u16string_view text) const
    {
        std::vector<Line> lines;
        std::size_t lineStart = 0;

        while (true)
        {
            TakenLine taken = TakeLine(text, lineStart);
            lines.push_back(taken.line);
            if (taken.isLastLine)
                return lines;

            lineStart = taken.nextLineStart;
        }
    }

    // The menu measures a text with line breaks as one line height for each break plus the height of the font for the last line.
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
    void AddGlyphsOfLine(const Line& line, std::size_t lineNumber, const TextArea& block, LaidOutText& laidOutText) const
    {
        float freeWidth = block.width - line.width;
        float lineLeft = block.left + freeWidth * FreeSpaceBefore(style.alignment.horizontal);
        float lineTop = block.top - static_cast<float>(lineNumber) * lineHeight;
        float baselineY = lineTop - font.ascent * scaleY;

        float penX = lineLeft;
        bool followsGlyph = false;
        for (char16_t character : line.characters)
        {
            if (IsControlCode(character))
                continue;

            if (followsGlyph)
                penX += style.characterSpace;

            laidOutText.glyphs.push_back(MakeGlyph(character, penX, baselineY));
            penX += AdvanceOf(character);
            followsGlyph = true;
        }
    }

    // The quad starts the left spacing of the glyph after the pen and is as wide as the glyph. It reaches from the baseline position
    // of the font above the baseline down to the bottom of the cell. The texture coordinates cut out exactly that part of the sheet.
    PlacedGlyph MakeGlyph(char16_t character, float penX, float baselineY) const
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

    const Formats::Font& font;
    const GlyphLookup& glyphLookup;
    const TextStyle& style;
    const TextArea& area;

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
    LayoutJob job(font, glyphLookup, style, area, paneAlpha);
    return job.Run(text);
}

}
