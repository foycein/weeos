#pragma once

#include "Channels/PagePainter.h"

#include "Browser/WebPage.h"
#include "Graphics/GraphicsDevice.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

namespace Channels
{

// The web page of the Internet Channel: Chromium's page (Browser::WebPage) and its newest picture, drawn into the top left part of the
// picture. One layout unit is one pixel of the page, so the page is as wide as the picture (832 pixels in 16:9).
class PageView
{
public:
    // The engine draws two of its pixels for every layout unit, so the page stays sharp in a window up to 1664 pixels wide and is
    // still smooth in a full 4K screen (Assumed, docs/audit/Browser.md).
    static constexpr double PixelsPerUnit = 2;

    // The steps of the zoom of + and - (Assumed: the original's steps were not read).
    static constexpr std::array<double, 6> ZoomSteps = {1.0, 1.25, 1.5, 2.0, 2.5, 3.0};

    // The device must outlive the view. The engine starts in the background.
    PageView(Graphics::GraphicsDevice& device, const Browser::BrowserSettings& settings, const Browser::AddressRedirect& redirect);

    Browser::WebPage& Page();

    // The size of the page in layout units. A new size makes the engine lay the page out again.
    void SetArea(float width, float height);
    bool Contains(Formats::Vector2 pagePoint) const;

    // Takes the newest picture of the engine.
    void Update();
    void Draw(PagePainter& painter) const;

    // The pointer is only sent when it moved, so that the engine is not asked sixty times a second for nothing.
    void MovePointer(Formats::Vector2 pagePoint);

    void ZoomIn();
    void ZoomOut();

private:
    Graphics::GraphicsDevice& device;
    std::unique_ptr<Browser::WebPage> page;
    std::unique_ptr<PagePicture> picture;
    float areaWidth = 0;
    float areaHeight = 0;
    std::optional<Formats::Vector2> sentPointer;
    std::size_t zoomStep = 0;
};

}
