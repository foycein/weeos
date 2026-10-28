#pragma once

#include "Layout/PanePlacement.h"

#include "Formats/Layout.h"
#include "Formats/Vectors.h"

#include <vector>

namespace Layout
{

// Finds the bounding panes that contain a point of the layout, the one drawn last (in front) first.
// The point is given in layout units: the origin in the middle, x to the right and y upward.
// A pane that is not shown, or that has a pane above it that is not shown, is never found.
// The settings are the ones the layout is drawn with.
std::vector<const Formats::Pane*> FindBoundingPanesAt(const Formats::Pane& rootPane, Formats::Vector2 point,
    const PlacementSettings& settings = PlacementSettings());

}
