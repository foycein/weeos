#pragma once

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/TitleContents.h"

#include "Formats/Font.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/TextFonts.h"
#include "Text/TextDrawer.h"
#include "Text/TextLayouter.h"

#include <memory>
#include <string>
#include <vector>

namespace SystemMenu
{

// The fonts that the text boxes of the layouts are drawn with. A layout names its fonts by file name, and this says which font each name means.
class MenuFonts
{
public:
    // Reads the three fonts of the menu: the two bitmap fonts of the font content and the large font of the resource archive.
    // The device and the files must stay alive as long as this object.
    MenuFonts(Graphics::GraphicsDevice& graphicsDevice, TitleContents& titleContents, const MenuFiles& menuFiles);

    // Makes a font available under the names that layouts use for it.
    void AddFont(Formats::Font font, const std::vector<std::string>& names);

    const Layout::TextFonts& Fonts() const;

private:
    Graphics::GraphicsDevice& device;
    std::vector<std::unique_ptr<Formats::Font>> fonts;
    std::vector<std::unique_ptr<Text::TextLayouter>> layouters;
    std::vector<std::unique_ptr<Text::TextDrawer>> drawers;
    Layout::TextFonts textFonts;
};

}
