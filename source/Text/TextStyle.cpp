#include "Text/TextStyle.h"

#include "Formats/Layout.h"
#include "Text/TextError.h"

#include <algorithm>
#include <format>

namespace Text
{

namespace
{

constexpr int AlignmentColumnCount = 3;
constexpr int AlignmentRowCount = 3;

constexpr std::int16_t LowestColorValue = 0;
constexpr std::int16_t HighestColorValue = 255;

Formats::Byte CutToByte(std::int16_t value)
{
    return static_cast<Formats::Byte>(std::clamp(value, LowestColorValue, HighestColorValue));
}

Formats::Rgba8Color ColorOfRegister(const Formats::SignedColor& registerColor)
{
    return {CutToByte(registerColor.red), CutToByte(registerColor.green), CutToByte(registerColor.blue), CutToByte(registerColor.alpha)};
}

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

TextStyle MakeTextStyle(const Formats::TextBox& textBox, const Formats::Material& material)
{
    TextStyle style;
    style.fontSize = textBox.fontSize;
    style.characterSpace = textBox.characterSpace;
    style.lineSpace = textBox.lineSpace;
    style.topColor = textBox.topColor;
    style.bottomColor = textBox.bottomColor;
    style.blankColor = ColorOfRegister(material.foreColor);
    style.inkColor = ColorOfRegister(material.backColor);
    style.alignment = TextAlignmentFromNumber(textBox.textOrigin);
    return style;
}

}
