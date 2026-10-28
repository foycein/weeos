#include "Layout/HitTest.h"

#include "Layout/PaneRectangle.h"

#include <optional>

namespace Layout
{

namespace
{

bool PlacementContainsPoint(const PanePlacement& placement, Formats::Vector2 point)
{
    std::optional<Formats::Vector2> pointInPane = FindPointMovedTo(placement.matrix, point);
    if (!pointInPane)
        return false;

    return Contains(RectangleOfPane(*placement.pane), *pointInPane);
}

}

std::vector<const Formats::Pane*> FindBoundingPanesAt(const Formats::Pane& rootPane, Formats::Vector2 point, const PlacementSettings& settings)
{
    std::vector<PanePlacement> placements = PlaceShownPanes(rootPane, settings);

    std::vector<const Formats::Pane*> hitPanes;
    for (auto placement = placements.rbegin(); placement != placements.rend(); ++placement)
    {
        if (placement->pane->kind == Formats::PaneKind::Boundary && PlacementContainsPoint(*placement, point))
            hitPanes.push_back(placement->pane);
    }
    return hitPanes;
}

}
