#pragma once

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/TitleContents.h"

#include "Formats/Font.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/TextFonts.h"
#include "Text/TextDrawer.h"
#include "Text/TextLayouter.h"

#include <initializer_list>
#include <memory>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The three fonts of the menu, ready to draw the text boxes of the layouts. A layout names its font by a file name, and several
// names mean one font (docs/audit/OpenPoints/Text.md TXT-04).
class MenuFonts
{
public:
    // The fonts are in the shared content of the title that holds the fonts, and in the resource archive.
    // The device, the contents and the files are only used here. The device must stay alive as long as the fonts.
    MenuFonts(Graphics::GraphicsDevice& device, TitleContents& titleContents, const MenuFiles& files);

    // No fonts yet. A channel adds the fonts of its own files.
    MenuFonts() = default;

    // The font is drawn for each of the names. The device must stay alive as long as the fonts.
    void AddFont(Graphics::GraphicsDevice& device, Formats::Font font, std::initializer_list<std::string_view> layoutNames);

    const Layout::TextFonts& Fonts() const;

private:
    struct LoadedFont
    {
        Formats::Font font;
        Text::TextLayouter layouter;
        Text::TextDrawer drawer;

        LoadedFont(Graphics::GraphicsDevice& device, Formats::Font loadedFont);
    };

    std::vector<std::unique_ptr<LoadedFont>> loadedFonts;
    Layout::TextFonts textFonts;
};

}
