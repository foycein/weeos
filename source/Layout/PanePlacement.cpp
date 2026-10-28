#include "Layout/PanePlacement.h"

namespace Layout
{

namespace
{

constexpr std::uint8_t FullAlpha = 255;
constexpr float OneOverFullAlpha = 1.0f / 255.0f;

// The alpha that the panes above hand down to a pane.
struct InheritedAlpha
{
    bool isActive = false;

    // The alpha of the panes above, multiplied together, where 1 is fully opaque.
    float factor = 1.0f;
};

TransformMatrix MakeLocalMatrix(const Formats::Pane& pane, const PlacementSettings& settings)
{
    float scaleX = pane.scale.x;
    float scaleY = pane.scale.y;
    if (pane.adjustsLocationForWideScreen)
    {
        scaleX *= settings.locationAdjustScale.x;
        scaleY *= settings.locationAdjustScale.y;
    }

    return MakeTranslation(pane.translation.x, pane.translation.y, pane.translation.z)
        * MakeRotationAroundZ(pane.rotation.z)
        * MakeRotationAroundY(pane.rotation.y)
        * MakeRotationAroundX(pane.rotation.x)
        * MakeScale(scaleX, scaleY, 1.0f);
}

std::uint8_t DrawnAlpha(const Formats::Pane& pane, const InheritedAlpha& inherited)
{
    if (!inherited.isActive)
        return pane.alpha;

    float scaledAlpha = pane.alpha * inherited.factor;
    return static_cast<std::uint8_t>(static_cast<int>(scaledAlpha));
}

InheritedAlpha AlphaHandedDownBy(const Formats::Pane& pane, InheritedAlpha inherited)
{
    if (!pane.inheritsAlpha || pane.alpha == FullAlpha)
        return inherited;

    inherited.isActive = true;
    inherited.factor = OneOverFullAlpha * inherited.factor * pane.alpha;
    return inherited;
}

void PlacePane(const Formats::Pane& pane, const TransformMatrix& parentMatrix, const InheritedAlpha& inheritedAlpha,
    const PlacementSettings& settings, std::vector<PanePlacement>& placements)
{
    if (!pane.isVisible)
        return;

    PanePlacement placement;
    placement.pane = &pane;
    placement.matrix = parentMatrix * MakeLocalMatrix(pane, settings);
    placement.alpha = DrawnAlpha(pane, inheritedAlpha);
    placements.push_back(placement);

    InheritedAlpha alphaForChildren = AlphaHandedDownBy(pane, inheritedAlpha);
    for (const Formats::Pane& child : pane.children)
        PlacePane(child, placement.matrix, alphaForChildren, settings, placements);
}

}

std::vector<PanePlacement> PlaceShownPanes(const Formats::Pane& rootPane, const PlacementSettings& settings)
{
    std::vector<PanePlacement> placements;
    PlacePane(rootPane, TransformMatrix(), InheritedAlpha(), settings, placements);
    return placements;
}

}
