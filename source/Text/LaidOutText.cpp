#include "Text/LaidOutText.h"

#include "Text/TextError.h"

#include <format>

namespace Text
{

namespace
{

// The rectangle of one line from the pen position left to the pen position right.
TextArea AreaOfLine(const PlacedLine& line, float left, float right)
{
    TextArea area;
    area.left = left;
    area.top = line.top;
    area.width = right - left;
    area.height = line.height;
    return area;
}

// The place of a text cursor in front of the character, or behind the last character when the index is the end of the text.
TextArea AreaOfCursor(const LaidOutText& text, std::size_t characterIndex)
{
    if (characterIndex < text.characters.size())
    {
        const PlacedCharacter& character = text.characters[characterIndex];
        return AreaOfLine(text.lines[character.lineNumber], character.left, character.left);
    }

    const PlacedLine& lastLine = text.lines.back();
    float endOfLine = lastLine.left + lastLine.width;
    return AreaOfLine(lastLine, endOfLine, endOfLine);
}

}

std::vector<TextArea> AreasOfCharacters(const LaidOutText& text, std::size_t first, std::size_t end)
{
    if (text.lines.empty())
        throw TextError("the text has no lines");

    if (first > end || end > text.characters.size())
        throw TextError(std::format("the characters {} to {} are not in a text of {} characters", first, end, text.characters.size()));

    if (first == end)
        return {AreaOfCursor(text, first)};

    const PlacedCharacter& firstCharacter = text.characters[first];
    const PlacedCharacter& lastCharacter = text.characters[end - 1];

    std::vector<TextArea> areas;
    for (std::size_t lineNumber = firstCharacter.lineNumber; lineNumber <= lastCharacter.lineNumber; ++lineNumber)
    {
        const PlacedLine& line = text.lines[lineNumber];

        float left = line.left;
        if (lineNumber == firstCharacter.lineNumber)
            left = firstCharacter.left;

        float right = line.left + line.width;
        if (lineNumber == lastCharacter.lineNumber)
            right = lastCharacter.left + lastCharacter.advance;

        areas.push_back(AreaOfLine(line, left, right));
    }
    return areas;
}

}
