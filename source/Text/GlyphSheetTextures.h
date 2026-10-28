#pragma once

#include "Formats/Font.h"
#include "Graphics/GraphicsDevice.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Text
{

// The glyph sheets of a font as textures on a graphics device. A font has up to 70 sheets and a text uses very few of them,
// so a sheet is uploaded the first time one of its glyphs is drawn. The textures are destroyed with this object.
class GlyphSheetTextures
{
public:
    // The device and the font must stay alive as long as this object.
    GlyphSheetTextures(Graphics::GraphicsDevice& graphicsDevice, const Formats::Font& fontToUpload);
    ~GlyphSheetTextures();

    GlyphSheetTextures(const GlyphSheetTextures&) = delete;
    GlyphSheetTextures& operator=(const GlyphSheetTextures&) = delete;

    // The material that draws glyphs of the sheet. The color is the color of the quad and the alpha is the alpha of the sheet
    // times the alpha of the quad, which is how the Wii draws its fonts.
    const Graphics::Material& MaterialOfSheet(std::size_t sheetIndex);

private:
    Graphics::Material MakeMaterial(std::size_t sheetIndex);

    Graphics::GraphicsDevice& device;
    const Formats::Font& font;

    // One entry for every sheet of the font. An entry is empty until its sheet is needed.
    std::vector<std::optional<Graphics::Material>> materials;
};

}
