#pragma once

#include "Layout/TransformMatrix.h"

#include "Formats/Layout.h"
#include "Graphics/GraphicsDevice.h"

#include <array>

namespace Layout
{

// A rectangle in the space of a pane. The x axis grows to the right and the y axis grows upward, like in the layout,
// so the rectangle covers x from left to left + width and y from top - height to top.
struct PaneRectangle
{
    float left = 0;
    float top = 0;
    float width = 0;
    float height = 0;
};

// The rectangle of the pane. The origin of the pane says which point of the rectangle lies at the position of the pane:
// the rectangle of a pane with the origin at the bottom center starts at the bottom center, in the middle of its width, and grows upward.
PaneRectangle RectangleOfPane(const Formats::Pane& pane);

// True when the point lies inside the rectangle or on its edge.
bool Contains(const PaneRectangle& rectangle, Formats::Vector2 point);

// The corners going around the rectangle in the order of the quad: left top, right top, right bottom, left bottom.
std::array<Formats::Vector2, Graphics::QuadCornerCount> CornersOf(const PaneRectangle& rectangle, const TransformMatrix& matrix);

}
