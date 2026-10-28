#include "Layout/LayoutDrawer.h"

#include "Layout/PaneRectangle.h"
#include "Layout/RectangleDrawing.h"
#include "Layout/WindowDrawing.h"

namespace Layout
{

namespace
{

void DrawPicture(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement)
{
    const Formats::Pane& pane = *placement.pane;

    RectangleAppearance appearance;
    appearance.materialIndex = pane.picture->materialIndex;
    appearance.vertexColors = pane.picture->vertexColors;
    appearance.textureCoordinateSets = pane.picture->textureCoordinateSets;
    DrawRectangle(device, layout, placement, RectangleOfPane(pane), appearance);
}

}

void DrawPlacedPane(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement)
{
    switch (placement.pane->kind)
    {
    case Formats::PaneKind::Picture:
        DrawPicture(device, layout, placement);
        return;

    case Formats::PaneKind::Window:
        DrawWindow(device, layout, placement);
        return;

    case Formats::PaneKind::TextBox:
    case Formats::PaneKind::Null:
    case Formats::PaneKind::Boundary:
        return;
    }
}

void DrawLayout(Graphics::GraphicsDevice& device, LiveLayout& layout, const PlacementSettings& settings)
{
    for (const PanePlacement& placement : PlaceShownPanes(layout.RootPane(), settings))
        DrawPlacedPane(device, layout, placement);
}

}
