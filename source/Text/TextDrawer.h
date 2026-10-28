#pragma once

#include "Formats/Font.h"
#include "Formats/Vectors.h"
#include "Graphics/GraphicsDevice.h"
#include "Text/GlyphSheetTextures.h"
#include "Text/LaidOutText.h"

namespace Text
{

// Moves the points of a laid-out text to where the pane is on the picture: the point (x, y) of the text lands at
// translation + x * xAxis + y * yAxis. The default leaves the points where they are.
struct TextTransform
{
    Formats::Vector2 xAxis = {1, 0};
    Formats::Vector2 yAxis = {0, 1};
    Formats::Vector2 translation = {0, 0};

    Formats::Vector2 Apply(Formats::Vector2 point) const;
};

// Draws laid-out text on a graphics device, one quad for every glyph, with the sheets of one font.
class TextDrawer
{
public:
    // The device and the font must stay alive as long as the drawer.
    TextDrawer(Graphics::GraphicsDevice& graphicsDevice, const Formats::Font& font);

    void Draw(const LaidOutText& text, const TextTransform& transform = TextTransform());

private:
    Graphics::GraphicsDevice& device;
    GlyphSheetTextures sheets;
    Formats::TextureFormat sheetFormat;
};

}
