#include "Channels/PagePainter.h"

#include "Text/LaidOutText.h"

namespace Channels
{

namespace
{

constexpr Formats::Rgba8Color OpaqueWhite = {255, 255, 255, 255};

}

bool PageRectangle::Contains(Formats::Vector2 pagePoint) const
{
    bool isInsideAcross = pagePoint.x >= left && pagePoint.x < left + width;
    bool isInsideDown = pagePoint.y >= top && pagePoint.y < top + height;
    return isInsideAcross && isInsideDown;
}

PagePicture::PagePicture(Graphics::GraphicsDevice& graphicsDevice, const Formats::RgbaImage& image)
    : device(graphicsDevice)
    , width(static_cast<float>(image.width))
    , height(static_cast<float>(image.height))
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

float PagePicture::Width() const
{
    return width;
}

float PagePicture::Height() const
{
    return height;
}

PagePainter::PagePainter(Graphics::GraphicsDevice& graphicsDevice, const Layout::DrawableFont& pageFont, PageFrame pageFrame)
    : device(graphicsDevice)
    , font(pageFont)
    , frame(pageFrame)
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

void PagePainter::DrawPicturePart(const PagePicture& picture, const PageRectangle& place, const PageRectangle& part)
{
    float left = part.left / picture.Width();
    float right = (part.left + part.width) / picture.Width();
    float top = part.top / picture.Height();
    float bottom = (part.top + part.height) / picture.Height();

    Graphics::Quad quad = QuadOf(place, OpaqueWhite);
    quad.textureCoordinates[0] = {Formats::Vector2{left, top}, {right, top}, {right, bottom}, {left, bottom}};

    Graphics::Material material;
    material.textures.push_back({picture.Texture(), Graphics::TextureMatrix()});
    device.DrawQuad(quad, material);
}

void PagePainter::DrawText(std::u16string_view text, const PageTextStyle& style, const PageRectangle& box)
{
    Text::TextStyle textStyle = TextStyleOf(style);
    Text::TextArea area = TextAreaOf(box);
    std::u16string wrappedText = WrapAtSpaces(text, textStyle, area.width);
    Text::LaidOutText laidOutText = font.layouter.LayOut(wrappedText, textStyle, area);
    font.drawer.Draw(laidOutText);
}

void PagePainter::DrawTextLine(std::u16string_view text, const PageTextStyle& style, const PageRectangle& box)
{
    Text::TextStyle textStyle = TextStyleOf(style);
    Text::TextArea area = TextAreaOf(box);
    Text::LaidOutText laidOutText = font.layouter.LayOut(CutWithEllipsis(text, textStyle, area.width), textStyle, area);
    font.drawer.Draw(laidOutText);
}

Text::TextStyle PagePainter::TextStyleOf(const PageTextStyle& style) const
{
    Text::TextStyle textStyle;
    textStyle.fontSize = {style.size * frame.scale, style.size * frame.scale};
    textStyle.topColor = style.color;
    textStyle.bottomColor = style.color;
    textStyle.alignment = {style.alignment, Text::VerticalAlignment::Top};
    return textStyle;
}

Text::TextArea PagePainter::TextAreaOf(const PageRectangle& box) const
{
    Formats::Vector2 topLeft = PageToPicture(box.left, box.top);
    Text::TextArea area;
    area.left = topLeft.x;
    area.top = topLeft.y;
    area.width = box.width * frame.scale;
    area.height = box.height * frame.scale;
    return area;
}

// The characters that fit together with "..." stay; the text stays whole when it fits.
std::u16string PagePainter::CutWithEllipsis(std::u16string_view text, const Text::TextStyle& style, float width) const
{
    constexpr std::u16string_view Ellipsis = u"...";
    Text::TextArea longLine;
    longLine.width = UnlimitedWidth;
    longLine.height = UnlimitedWidth;

    Text::LaidOutText laidOutText = font.layouter.LayOut(text, style, longLine);
    if (laidOutText.characters.empty())
        return std::u16string(text);

    const Text::PlacedCharacter& last = laidOutText.characters.back();
    float firstLeft = laidOutText.characters.front().left;
    if (last.left + last.advance - firstLeft <= width)
        return std::u16string(text);

    Text::LaidOutText ellipsis = font.layouter.LayOut(Ellipsis, style, longLine);
    float ellipsisWidth = 0;
    for (const Text::PlacedCharacter& character : ellipsis.characters)
        ellipsisWidth += character.advance;

    std::size_t keptCount = 0;
    while (keptCount < text.size())
    {
        const Text::PlacedCharacter& character = laidOutText.characters[keptCount];
        if (character.left + character.advance - firstLeft + ellipsisWidth > width)
            break;
        ++keptCount;
    }
    return std::u16string(text.substr(0, keptCount)) + std::u16string(Ellipsis);
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

Formats::Vector2 PagePainter::PictureToPage(Formats::Vector2 picturePoint) const
{
    return {picturePoint.x / frame.scale + frame.width / 2, frame.height / 2 - picturePoint.y / frame.scale};
}

Formats::Vector2 PagePainter::PageToPicture(float pageX, float pageY) const
{
    return {(pageX - frame.width / 2) * frame.scale, (frame.height / 2 - pageY) * frame.scale};
}

Graphics::Quad PagePainter::QuadOf(const PageRectangle& place, Formats::Rgba8Color color) const
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
