#pragma once

#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/TextFonts.h"
#include "Text/TextStyle.h"

#include <string_view>

namespace Channels
{

// The Shop and the Internet Channel show web pages, laid out by the browser in pixels of the page from its top left corner.
// WeeOS draws them natively. The Shop's pages are 608 by 456 pixels, one pixel is one layout unit, and the page lies in the middle of
// the picture.
constexpr float PageWidth = 608;
constexpr float PageHeight = 456;

// Where a page lies: its size in pixels of the page and how many layout units one pixel is. The page lies in the middle of the picture.
struct PageFrame
{
    float width = PageWidth;
    float height = PageHeight;
    float scale = 1;
};

struct PageRectangle
{
    float left = 0;
    float top = 0;
    float width = 0;
    float height = 0;

    bool Contains(Formats::Vector2 pagePoint) const;
};

// A picture of a page on the graphics device. It goes from the device with this object.
class PagePicture
{
public:
    PagePicture(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image);
    ~PagePicture();

    PagePicture(const PagePicture&) = delete;
    PagePicture& operator=(const PagePicture&) = delete;

    Graphics::TextureHandle Texture() const;
    float Width() const;
    float Height() const;

private:
    Graphics::GraphicsDevice& device;
    Graphics::TextureHandle texture;
    float width;
    float height;
};

// How a text of a page looks: its size in pixels, its color and where it sits in its box.
struct PageTextStyle
{
    float size = 16;
    Formats::Rgba8Color color = {0, 0, 0, 255};
    Text::HorizontalAlignment alignment = Text::HorizontalAlignment::Left;
};

// Draws the parts of a page: plain rectangles, pictures and text.
class PagePainter
{
public:
    // The device and the font must outlive the painter.
    PagePainter(Graphics::GraphicsDevice& device, const Layout::DrawableFont& font, PageFrame pageFrame = {});

    void FillRectangle(const PageRectangle& place, Formats::Rgba8Color color);

    // A picture that is mirrored is flipped from left to right.
    void DrawPicture(const PagePicture& picture, const PageRectangle& place, bool isMirrored = false);

    // A part of a picture, in pixels of the picture, as a page shows one state of a picture that holds several (a CSS sprite).
    void DrawPicturePart(const PagePicture& picture, const PageRectangle& place, const PageRectangle& part);

    // The text is broken into lines at the line breaks and at the right edge of the box.
    void DrawText(std::u16string_view text, const PageTextStyle& style, const PageRectangle& box);

    // One line that ends with "..." where it would go beyond the box, as text-overflow: ellipsis of a style sheet.
    void DrawTextLine(std::u16string_view text, const PageTextStyle& style, const PageRectangle& box);

    // A point of the picture, in layout units, as a point of the page.
    Formats::Vector2 PictureToPage(Formats::Vector2 picturePoint) const;

private:
    // Wide enough for the longest text of a page on one line.
    static constexpr float UnlimitedWidth = 100000;

    std::u16string WrapAtSpaces(std::u16string_view text, const Text::TextStyle& style, float width) const;
    std::u16string CutWithEllipsis(std::u16string_view text, const Text::TextStyle& style, float width) const;
    Text::TextStyle TextStyleOf(const PageTextStyle& style) const;
    Text::TextArea TextAreaOf(const PageRectangle& box) const;
    Formats::Vector2 PageToPicture(float pageX, float pageY) const;
    Graphics::Quad QuadOf(const PageRectangle& place, Formats::Rgba8Color color) const;

    Graphics::GraphicsDevice& device;
    const Layout::DrawableFont& font;
    PageFrame frame;
};

}
