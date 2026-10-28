#pragma once

#include "Formats/Font.h"
#include "Graphics/GraphicsDevice.h"

#include <cstddef>
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

    // The texture of the sheet. Where a glyph has ink, the alpha of the texture is its intensity.
    Graphics::TextureHandle TextureOfSheet(std::size_t sheetIndex);

private:
    Graphics::TextureHandle UploadSheet(std::size_t sheetIndex);

    Graphics::GraphicsDevice& device;
    const Formats::Font& font;

    // One handle for every sheet of the font. A handle that names nothing belongs to a sheet that was not needed yet.
    std::vector<Graphics::TextureHandle> textures;
};

}
