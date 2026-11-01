#pragma once

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/TitleContents.h"

#include "Formats/Font.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/TextFonts.h"
#include "Text/TextDrawer.h"
#include "Text/TextLayouter.h"

#include <memory>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The content of the System Menu title that holds the two system fonts wbf1.brfna and wbf2.brfna.
constexpr std::uint16_t FontContentIndex = 5;

// The three fonts of the menu, ready to draw text boxes. The layouts name their fonts by file name, and each name means one of these fonts.
class MenuFonts
{
public:
    // Reads the fonts from the System Menu title. The glyph sheets go to the device when the first glyph of a sheet is drawn.
    // The device must outlive the fonts.
    MenuFonts(Graphics::GraphicsDevice& device, TitleContents& titleContents, const MenuFiles& files);

    const Layout::TextFonts& Fonts() const;

private:
    // A font with what is needed to draw with it. The layouter and the drawer keep a reference to the font, so it stays where it is.
    struct DrawableFontSet
    {
        DrawableFontSet(Graphics::GraphicsDevice& device, Formats::Font fontToDraw);

        Formats::Font font;
        Text::TextLayouter layouter;
        Text::TextDrawer drawer;
    };

    void AddFont(Graphics::GraphicsDevice& device, Formats::Font font, std::initializer_list<std::string_view> layoutNames);

    std::vector<std::unique_ptr<DrawableFontSet>> fontSets;
    Layout::TextFonts fonts;
};

}
