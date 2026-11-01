#pragma once

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/WiiSettings/PageContent.h"

#include "Formats/Font.h"
#include "Graphics/GraphicsDevice.h"
#include "Text/TextDrawer.h"
#include "Text/TextLayouter.h"

#include <string>

namespace SystemMenu
{

// The font of the Wii Settings pages: the first small font of the menu, drawn smaller or larger. The pages of the original use a font of their own for
// the web browser; this is the font of the menu that looks the closest to it.
class PageFont
{
public:
    // The device and the files must outlive the font. Throws a MenuError when the font is not in the files.
    PageFont(Graphics::GraphicsDevice& device, const MenuFiles& files);

    PageFont(const PageFont&) = delete;
    PageFont& operator=(const PageFont&) = delete;

    // Draws the text in the rectangle of the page. The size is the height of a line of text in points of the page.
    void Draw(const std::u16string& text, const PageRectangle& bounds, const Text::TextAlignment& alignment, const Formats::Rgba8Color& color,
        float sizeInPoints);

private:
    Formats::Font font;
    Text::TextLayouter layouter;
    Text::TextDrawer drawer;
};

}
