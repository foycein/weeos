#include "Text/TextStyle.h"

#include "Formats/Layout.h"
#include "Text/TextError.h"

#include <format>

namespace Text
{

namespace
{

constexpr int AlignmentColumnCount = 3;
constexpr int AlignmentRowCount = 3;

}

TextAlignment TextAlignmentFromNumber(std::uint8_t number)
{
    if (number >= AlignmentColumnCount * AlignmentRowCount)
        throw TextError(std::format("the text position {} is not one of the nine positions", number));

    TextAlignment alignment;
    alignment.horizontal = static_cast<HorizontalAlignment>(number % AlignmentColumnCount);
    alignment.vertical = static_cast<VerticalAlignment>(number / AlignmentColumnCount);
    return alignment;
}

TextStyle MakeTextStyle(const Formats::TextBox& textBox)
{
    TextStyle style;
    style.fontSize = textBox.fontSize;
    style.characterSpace = textBox.characterSpace;
    style.lineSpace = textBox.lineSpace;
    style.topColor = textBox.topColor;
    style.bottomColor = textBox.bottomColor;
    style.alignment = TextAlignmentFromNumber(textBox.textOrigin);
    return style;
}

}
