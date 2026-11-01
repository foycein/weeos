#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "SystemMenu/MenuError.h"

#include "Layout/LayoutDrawer.h"
#include "Layout/PaneRectangle.h"
#include "Layout/TransformMatrix.h"

#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view RootPaneName = "RootPane";
constexpr std::size_t FirstTextureMap = 0;

Layout::LiveMaterial& MaterialOfPicture(MenuLayout& layout, std::string_view pictureName)
{
    Formats::Pane& pane = layout.PaneNamed(pictureName);
    if (!pane.picture)
        throw MenuError(std::format("The pane {} is not a picture", pictureName));

    return layout.Live().MaterialAt(pane.picture->materialIndex);
}

}

PlacedPanes::PlacedPanes(MenuLayout& layoutToPlace)
    : layout(layoutToPlace)
    , placements(Layout::PlaceShownPanes(layoutToPlace.Live().RootPane(), layoutToPlace.Placement()))
{
}

const std::vector<Layout::PanePlacement>& PlacedPanes::InDrawingOrder() const
{
    return placements;
}

std::optional<Formats::Vector2> PlacedPanes::PlaceOf(std::string_view paneName) const
{
    const Layout::PanePlacement* placement = Find(paneName);
    if (placement == nullptr)
        return std::nullopt;

    return Layout::TransformPoint(placement->matrix, {0, 0});
}

bool PlacedPanes::IsOnPane(std::string_view paneName, Formats::Vector2 place) const
{
    const Layout::PanePlacement* placement = Find(paneName);
    if (placement == nullptr)
        return false;

    std::optional<Formats::Vector2> placeInPane = Layout::FindPointMovedTo(placement->matrix, place);
    if (!placeInPane)
        return false;

    return Layout::Contains(Layout::RectangleOfPane(*placement->pane), *placeInPane);
}

const Layout::PanePlacement* PlacedPanes::Find(std::string_view paneName) const
{
    const Formats::Pane* pane = &layout.PaneNamed(paneName);
    for (const Layout::PanePlacement& placement : placements)
    {
        if (placement.pane == pane)
            return &placement;
    }
    return nullptr;
}

void DrawPanes(MenuLayout& layout, Display& display, const PlacedPanes& placedPanes, const PaneFilter& isDrawn)
{
    for (const Layout::PanePlacement& placement : placedPanes.InDrawingOrder())
    {
        if (isDrawn(*placement.pane))
            Layout::DrawPlacedPane(display.Device(), layout.Live(), placement, display.TextFonts());
    }
}

void MoveLayoutTo(MenuLayout& layout, Formats::Vector2 place)
{
    layout.PaneNamed(RootPaneName).translation = {place.x, place.y, 0};
}

void CopyFirstTexture(MenuLayout& layout, std::string_view toPictureName, std::string_view fromPictureName)
{
    Layout::LiveMaterial& source = MaterialOfPicture(layout, fromPictureName);
    Layout::LiveMaterial& target = MaterialOfPicture(layout, toPictureName);
    if (source.textureNames.empty() || target.textureNames.empty())
        throw MenuError(std::format("The pictures {} and {} need a texture each", fromPictureName, toPictureName));

    target.textureNames[FirstTextureMap] = source.textureNames[FirstTextureMap];
}

Layout::PlaybackMode ModeOfAnimationFile(const Layout::LayoutArchive& archive, std::string_view animationName)
{
    bool isLooping = archive.ReadAnimation(std::string(animationName)).isLooping;
    return isLooping ? Layout::PlaybackMode::Loop : Layout::PlaybackMode::PlayOnce;
}

}
