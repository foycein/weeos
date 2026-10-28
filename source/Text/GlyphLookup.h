#pragma once

#include "Formats/Font.h"

#include <cstdint>
#include <vector>

namespace Text
{

// Finds the glyph of a character in one step. A font lists its characters in several maps of three kinds
// (direct, table and scan). The lookup reads all maps once into one table. The first map whose range holds a character decides.
class GlyphLookup
{
public:
    explicit GlyphLookup(const Formats::Font& font);

    bool HasGlyph(char16_t character) const;

    // The glyph of the character. A character that the font has no glyph for gets the replacement glyph of the font.
    std::uint16_t FindGlyphIndex(char16_t character) const;

private:
    // One entry for every character code: the glyph, or a number that says that the font has none.
    std::vector<std::uint16_t> glyphIndices;

    std::uint16_t replacementGlyphIndex;
};

}
