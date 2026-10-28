#pragma once

#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"

#include <cstdint>

namespace Formats
{
struct TextBox;
}

namespace Text
{

enum class HorizontalAlignment : std::uint8_t
{
    Left,
    Center,
    Right,
};

enum class VerticalAlignment : std::uint8_t
{
    Top,
    Middle,
    Bottom,
};

// Where the block of lines sits inside its area. The lines inside the block follow the horizontal alignment too:
// with Center every line is centered in the block, with Right every line ends at the right edge of the block.
struct TextAlignment
{
    HorizontalAlignment horizontal = HorizontalAlignment::Center;
    VerticalAlignment vertical = VerticalAlignment::Middle;
};

// What a text looks like, apart from the string and the place.
struct TextStyle
{
    // The size of a character in layout units. A font draws at its own size when this is (font width, font height).
    Formats::Vector2 fontSize = {0, 0};

    // Extra space after each character and after each line. It can be negative.
    float characterSpace = 0;
    float lineSpace = 0;

    // The colors at the top and at the bottom of every glyph. Different colors make a vertical gradient.
    Formats::Rgba8Color topColor = {255, 255, 255, 255};
    Formats::Rgba8Color bottomColor = {255, 255, 255, 255};

    TextAlignment alignment;
};

// The rectangle that a text fills, in layout units. x grows to the right and y grows upward, so the bottom edge is at top - height.
struct TextArea
{
    float left = 0;
    float top = 0;
    float width = 0;
    float height = 0;
};

// The layout files store the alignment as one number: vertical * 3 + horizontal.
TextAlignment TextAlignmentFromNumber(std::uint8_t number);

// The style that a text box of a layout file asks for.
TextStyle MakeTextStyle(const Formats::TextBox& textBox);

}
