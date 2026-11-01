#pragma once

#include "SystemMenu/WiiSettings/PageContent.h"
#include "SystemMenu/WiiSettings/PageFont.h"
#include "SystemMenu/WiiSettings/PageImages.h"

#include "Graphics/GraphicsDevice.h"

#include <optional>

namespace SystemMenu
{

// Draws a page of Wii Settings with its pictures and its font: the background across the whole picture, the tabs, the buttons, the texts.
class PagePainter
{
public:
    // The device, the pictures and the font must outlive the painter.
    PagePainter(Graphics::GraphicsDevice& graphicsDevice, PageImages& pageImages, PageFont& pageFont);

    // The pointed button is the one that the pointer is on. A page is 608 by 456 points, and the canvas can be wider.
    void Draw(const Page& page, std::optional<std::size_t> pointedButton, const Graphics::Canvas& canvas);

private:
    void DrawBackground(const Graphics::Canvas& canvas);
    void DrawTabs(const Page& page);
    void DrawPageNumbers(const Page& page);
    void DrawButton(const PageButton& button, bool isPointed);
    void DrawTexts(const Page& page);

    // Draws a picture at the place, stretched to its size, in the color of the vertex colors.
    void DrawPicture(const std::string& imagePath, const PageRectangle& bounds, const Formats::Rgba8Color& tint);
    void DrawPictureAtItsOwnSizeInTheMiddle(const std::string& imagePath, const PageRectangle& bounds, const Formats::Rgba8Color& tint);

    Graphics::GraphicsDevice& device;
    PageImages& images;
    PageFont& font;
};

}
