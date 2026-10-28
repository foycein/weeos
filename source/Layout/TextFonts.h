#pragma once

#include "Text/TextDrawer.h"
#include "Text/TextLayouter.h"

#include <map>
#include <string>

namespace Layout
{

// A font that is ready to draw text: the layouter places the glyphs and the drawer sends them to the graphics device.
struct DrawableFont
{
    const Text::TextLayouter& layouter;
    Text::TextDrawer& drawer;
};

// The fonts that text boxes are drawn with. A layout names its fonts by file name, for example "RevoIpl_RodinNTLGPro_DB_32_I4.brfnt",
// and the program says which font each name means. Several names can mean the same font.
class TextFonts
{
public:
    // The layouter and the drawer must stay alive as long as this object. Throws a LayoutError when the name was added before.
    void Add(const std::string& fontName, const Text::TextLayouter& layouter, Text::TextDrawer& drawer);

    // Throws a LayoutError when no font was added under the name.
    const DrawableFont& Find(const std::string& fontName) const;

private:
    std::map<std::string, DrawableFont> fonts;
};

}
