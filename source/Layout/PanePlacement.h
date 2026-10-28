#pragma once

#include "Layout/TransformMatrix.h"

#include "Formats/Layout.h"

#include <cstdint>
#include <vector>

namespace Layout
{

// Where a pane is and how opaque it is, once the panes above it have had their say.
struct PanePlacement
{
    const Formats::Pane* pane = nullptr;

    // Moves the space of the pane into the space of the layout.
    TransformMatrix matrix;

    // The alpha the pane is drawn with.
    std::uint8_t alpha = 255;
};

struct PlacementSettings
{
    // Panes that adjust their location for wide screens multiply their own scale by this. The scale (1, 1) changes nothing.
    // The program that draws a layout decides which layouts get another scale, depending on the shape of the screen.
    Formats::Vector2 locationAdjustScale = {1.0f, 1.0f};
};

// Walks through the tree of panes, parents first and then their children in the order of the file. That is the order
// in which the panes are drawn, the first one at the back. A pane that is not visible is left out together with all its children.
//
// The matrix of a pane is its parent's matrix times its own: first scaled, then turned around x, y and z, then moved.
//
// The alpha of a pane is its own alpha. A pane that passes its alpha on, and whose alpha is not the full 255, makes all panes
// below it more transparent: it scales their alpha by its alpha divided by 255. The scaled alpha is rounded down.
std::vector<PanePlacement> PlaceShownPanes(const Formats::Pane& rootPane, const PlacementSettings& settings = PlacementSettings());

}
