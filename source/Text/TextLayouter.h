#pragma once

#include "Formats/Font.h"
#include "Text/GlyphLookup.h"
#include "Text/LaidOutText.h"
#include "Text/TextStyle.h"

#include <cstdint>
#include <string_view>

namespace Text
{

// Turns a string into glyph quads, the way the text boxes of the System Menu draw: the lines are split at the line breaks and at
// the right edge of the area, the block of lines is placed in the area, and every character becomes one quad.
// The layouter knows nothing about the graphics device.
class TextLayouter
{
public:
    // The font must stay alive as long as the layouter.
    explicit TextLayouter(const Formats::Font& font);

    // The pane alpha (0 to 255) scales the alpha of both colors of the style.
    LaidOutText LayOut(std::u16string_view text, const TextStyle& style, const TextArea& area, std::uint8_t paneAlpha = 255) const;

private:
    const Formats::Font& font;
    GlyphLookup glyphLookup;
};

}
