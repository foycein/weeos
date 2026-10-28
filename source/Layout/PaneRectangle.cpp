#include "Layout/PaneRectangle.h"

namespace Layout
{

namespace
{

// How far the left edge of the rectangle lies from the position of the pane.
float LeftEdgeOffset(Formats::HorizontalOrigin origin, float width)
{
    switch (origin)
    {
    case Formats::HorizontalOrigin::Left:
        return 0.0f;
    case Formats::HorizontalOrigin::Center:
        return -width * 0.5f;
    case Formats::HorizontalOrigin::Right:
        return -width;
    }
    return 0.0f;
}

// How far the top edge of the rectangle lies above the position of the pane.
float TopEdgeOffset(Formats::VerticalOrigin origin, float height)
{
    switch (origin)
    {
    case Formats::VerticalOrigin::Top:
        return 0.0f;
    case Formats::VerticalOrigin::Center:
        return height * 0.5f;
    case Formats::VerticalOrigin::Bottom:
        return height;
    }
    return 0.0f;
}

}

PaneRectangle RectangleOfPane(const Formats::Pane& pane)
{
    PaneRectangle rectangle;
    rectangle.left = LeftEdgeOffset(pane.horizontalOrigin, pane.width);
    rectangle.top = TopEdgeOffset(pane.verticalOrigin, pane.height);
    rectangle.width = pane.width;
    rectangle.height = pane.height;
    return rectangle;
}

bool Contains(const PaneRectangle& rectangle, Formats::Vector2 point)
{
    bool isBetweenLeftAndRight = rectangle.left <= point.x && point.x <= rectangle.left + rectangle.width;
    bool isBetweenBottomAndTop = rectangle.top - rectangle.height <= point.y && point.y <= rectangle.top;
    return isBetweenLeftAndRight && isBetweenBottomAndTop;
}

std::array<Formats::Vector2, Graphics::QuadCornerCount> CornersOf(const PaneRectangle& rectangle, const TransformMatrix& matrix)
{
    float right = rectangle.left + rectangle.width;
    float bottom = rectangle.top - rectangle.height;

    return {
        TransformPoint(matrix, {rectangle.left, rectangle.top}),
        TransformPoint(matrix, {right, rectangle.top}),
        TransformPoint(matrix, {right, bottom}),
        TransformPoint(matrix, {rectangle.left, bottom}),
    };
}

}
