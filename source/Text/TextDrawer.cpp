#include "Text/TextDrawer.h"

namespace Text
{

Formats::Vector2 TextTransform::Apply(Formats::Vector2 point) const
{
    Formats::Vector2 movedPoint;
    movedPoint.x = translation.x + point.x * xAxis.x + point.y * yAxis.x;
    movedPoint.y = translation.y + point.x * xAxis.y + point.y * yAxis.y;
    return movedPoint;
}

TextDrawer::TextDrawer(Graphics::GraphicsDevice& graphicsDevice, const Formats::Font& font)
    : device(graphicsDevice)
    , sheets(graphicsDevice, font)
{
}

void TextDrawer::Draw(const LaidOutText& text, const TextTransform& transform)
{
    for (const PlacedGlyph& glyph : text.glyphs)
    {
        Graphics::Quad quad = glyph.quad;
        for (Formats::Vector2& position : quad.positions)
            position = transform.Apply(position);

        device.DrawQuad(quad, sheets.MaterialOfSheet(glyph.sheetIndex));
    }
}

}
