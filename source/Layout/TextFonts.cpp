#include "Layout/TextFonts.h"

#include "Layout/LayoutError.h"

#include <format>

namespace Layout
{

void TextFonts::Add(const std::string& fontName, const Text::TextLayouter& layouter, Text::TextDrawer& drawer)
{
    bool isNew = fonts.emplace(fontName, DrawableFont{layouter, drawer}).second;
    if (!isNew)
        throw LayoutError(std::format("The font {} was added twice", fontName));
}

const DrawableFont& TextFonts::Find(const std::string& fontName) const
{
    auto font = fonts.find(fontName);
    if (font == fonts.end())
        throw LayoutError(std::format("The program has no font {}", fontName));

    return font->second;
}

}
