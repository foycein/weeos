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

    // The positions are in the layout units of the text area. The colors are the colors of the style, with the pane alpha applied.
    // The texture coordinates are in set 0.
    Graphics::Quad quad;
};

struct LaidOutText
{
    std::vector<PlacedGlyph> glyphs;

    // The place of all lines together inside the text area.
    TextArea block;
};

}
