#include "Channels/Internet/PageView.h"

#include <cmath>

namespace Channels
{

PageView::PageView(Graphics::GraphicsDevice& graphicsDevice, const Browser::BrowserSettings& settings, const Browser::AddressRedirect& redirect)
    : device(graphicsDevice)
    , page(std::make_unique<Browser::WebPage>(settings))
{
    page->SetRedirect(redirect);
}

Browser::WebPage& PageView::Page()
{
    return *page;
}

void PageView::SetArea(float width, float height)
{
    if (width == areaWidth && height == areaHeight)
        return;

    areaWidth = width;
    areaHeight = height;
    int pageWidth = static_cast<int>(std::lround(width));
    int pageHeight = static_cast<int>(std::lround(height));
    page->SetViewSize({pageWidth, pageHeight, PixelsPerUnit});
}

bool PageView::Contains(Formats::Vector2 pagePoint) const
{
    return PageRectangle{0, 0, areaWidth, areaHeight}.Contains(pagePoint);
}

void PageView::Update()
{
    std::optional<Formats::RgbaImage> newPicture = page->TakeNewPicture();
    if (!newPicture)
        return;

    picture = std::make_unique<PagePicture>(device, *newPicture);
    latestPicture = std::move(newPicture);
}

void PageView::Draw(PagePainter& painter) const
{
    if (picture)
        painter.DrawPicture(*picture, {0, 0, areaWidth, areaHeight});
}

void PageView::MovePointer(Formats::Vector2 pagePoint)
{
    bool hasMoved = !sentPointer || sentPointer->x != pagePoint.x || sentPointer->y != pagePoint.y;
    if (!hasMoved)
        return;

    sentPointer = pagePoint;
    page->MovePointer(pagePoint.x, pagePoint.y);
}

void PageView::ZoomIn()
{
    if (zoomStep + 1 < ZoomSteps.size())
        ++zoomStep;
    page->SetZoom(ZoomSteps[zoomStep]);
}

void PageView::ResetZoom()
{
    zoomStep = 0;
    page->SetZoom(ZoomSteps[zoomStep]);
}

bool PageView::IsAtNormalZoom() const
{
    return zoomStep == 0;
}

const std::optional<Formats::RgbaImage>& PageView::LatestPicture() const
{
    return latestPicture;
}

void PageView::ZoomOut()
{
    if (zoomStep > 0)
        --zoomStep;
    page->SetZoom(ZoomSteps[zoomStep]);
}

}
