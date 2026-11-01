#pragma once

#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/TextFonts.h"
#include "Text/TextStyle.h"

#include <string_view>

namespace Channels
{

// The Shop and the Internet Channel show web pages, laid out by the browser in pixels of a page of 608 by 456 pixels,
// from its top left corner. WeeOS draws them natively: one pixel of the page is one layout unit, and the page lies in the middle of
// the picture.
constexpr float PageWidth = 608;
constexpr float PageHeight = 456;

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

private:
    Graphics::GraphicsDevice& device;
    Graphics::TextureHandle texture;
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
    PagePainter(Graphics::GraphicsDevice& device, const Layout::DrawableFont& font);

    void FillRectangle(const PageRectangle& place, Formats::Rgba8Color color);

    // A picture that is mirrored is flipped from left to right.
    void DrawPicture(const PagePicture& picture, const PageRectangle& place, bool isMirrored = false);

    // The text is broken into lines at the line breaks and at the right edge of the box.
    void DrawText(std::u16string_view text, const PageTextStyle& style, const PageRectangle& box);

    // A point of the picture, in layout units, as a point of the page.
    static Formats::Vector2 PictureToPage(Formats::Vector2 picturePoint);

private:
    // Wide enough for the longest text of a page on one line.
    static constexpr float UnlimitedWidth = 100000;

    std::u16string WrapAtSpaces(std::u16string_view text, const Text::TextStyle& style, float width) const;

    Graphics::GraphicsDevice& device;
    const Layout::DrawableFont& font;
};

}
