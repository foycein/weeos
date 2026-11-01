#include "Channels/PagePainter.h"

#include "Text/LaidOutText.h"

namespace Channels
{

namespace
{

constexpr Formats::Rgba8Color OpaqueWhite = {255, 255, 255, 255};

Formats::Vector2 PageToPicture(float pageX, float pageY)
{
    return {pageX - PageWidth / 2, PageHeight / 2 - pageY};
}

Graphics::Quad QuadOf(const PageRectangle& place, Formats::Rgba8Color color)
{
    float right = place.left + place.width;
    float bottom = place.top + place.height;

    Graphics::Quad quad{};
    quad.positions = {PageToPicture(place.left, place.top), PageToPicture(right, place.top), PageToPicture(right, bottom),
        PageToPicture(place.left, bottom)};
    quad.colors = {color, color, color, color};
    return quad;
}

}

bool PageRectangle::Contains(Formats::Vector2 pagePoint) const
{
    bool isInsideAcross = pagePoint.x >= left && pagePoint.x < left + width;
    bool isInsideDown = pagePoint.y >= top && pagePoint.y < top + height;
    return isInsideAcross && isInsideDown;
}

PagePicture::PagePicture(Graphics::GraphicsDevice& graphicsDevice, const Formats::RgbaImage& image)
    : device(graphicsDevice)
{
    Graphics::TextureDescription description;
    description.width = image.width;
    description.height = image.height;
    description.pixels = image.pixels;
    texture = device.CreateTexture(description);
}

PagePicture::~PagePicture()
{
    device.DestroyTexture(texture);
}

Graphics::TextureHandle PagePicture::Texture() const
{
    return texture;
}

PagePainter::PagePainter(Graphics::GraphicsDevice& graphicsDevice, const Layout::DrawableFont& pageFont)
    : device(graphicsDevice)
    , font(pageFont)
{
}

void PagePainter::FillRectangle(const PageRectangle& place, Formats::Rgba8Color color)
{
    device.DrawQuad(QuadOf(place, color), Graphics::Material());
}

void PagePainter::DrawPicture(const PagePicture& picture, const PageRectangle& place, bool isMirrored)
{
    Graphics::Quad quad = QuadOf(place, OpaqueWhite);
    if (isMirrored)
        quad.textureCoordinates[0] = {Formats::Vector2{1, 0}, {0, 0}, {0, 1}, {1, 1}};
    else
        quad.textureCoordinates[0] = {Formats::Vector2{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    Graphics::Material material;
    material.textures.push_back({picture.Texture(), Graphics::TextureMatrix()});
    device.DrawQuad(quad, material);
}

void PagePainter::DrawText(std::u16string_view text, const PageTextStyle& style, const PageRectangle& box)
{
    Text::TextStyle textStyle;
    textStyle.fontSize = {style.size, style.size};
    textStyle.topColor = style.color;
    textStyle.bottomColor = style.color;
    textStyle.alignment = {style.alignment, Text::VerticalAlignment::Top};

    Formats::Vector2 topLeft = PageToPicture(box.left, box.top);
    Text::TextArea area;
    area.left = topLeft.x;
    area.top = topLeft.y;
    area.width = box.width;
    area.height = box.height;

    std::u16string wrappedText = WrapAtSpaces(text, textStyle, box.width);
    Text::LaidOutText laidOutText = font.layouter.LayOut(wrappedText, textStyle, area);
    font.drawer.Draw(laidOutText);
}

// The browser breaks a line at the last space that fits, where the text boxes of the layouts break at any character.
// The text is laid out on one long line first, and a space becomes a line break where the next word would not fit.
std::u16string PagePainter::WrapAtSpaces(std::u16string_view text, const Text::TextStyle& style, float width) const
{
    Text::TextArea longLine;
    longLine.width = UnlimitedWidth;
    longLine.height = UnlimitedWidth;
    Text::LaidOutText laidOutText = font.layouter.LayOut(text, style, longLine);

    std::u16string wrappedText(text);
    std::size_t lastSpace = std::u16string::npos;
    float lineStart = 0;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const Text::PlacedCharacter& character = laidOutText.characters[index];
        bool startsLine = index == 0 || character.lineNumber != laidOutText.characters[index - 1].lineNumber;
        if (startsLine)
        {
            lineStart = character.left;
            lastSpace = std::u16string::npos;
        }

        bool isBeyondEdge = character.left + character.advance - lineStart > width;
        if (isBeyondEdge && lastSpace != std::u16string::npos)
        {
            wrappedText[lastSpace] = u'\n';
            lineStart = laidOutText.characters[lastSpace + 1].left;
            lastSpace = std::u16string::npos;
        }

        if (text[index] == u' ')
            lastSpace = index;
    }
    return wrappedText;
}

Formats::Vector2 PagePainter::PictureToPage(Formats::Vector2 picturePoint)
{
    return {picturePoint.x + PageWidth / 2, PageHeight / 2 - picturePoint.y};
}

}
