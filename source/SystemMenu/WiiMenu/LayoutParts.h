#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/MenuLayout.h"

#include "Formats/Vectors.h"
#include "Layout/AnimationPlayer.h"
#include "Layout/LayoutArchive.h"
#include "Layout/PanePlacement.h"

#include <functional>
#include <map>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The screens of the Wii Menu draw their layouts in parts: the channel grid draws the icons of the channels between the back
// of its layout and the front of it, and puts the clock and the balloon where panes of the layout are.

// From where to where a pane reaches across the picture.
struct HorizontalSpan
{
    float left = 0;
    float right = 0;
};

// The shown panes of a layout, placed once for one picture.
class PlacedPanes
{
public:
    explicit PlacedPanes(MenuLayout& layout);

    // In the order in which they are drawn, the first one at the back.
    const std::vector<Layout::PanePlacement>& InDrawingOrder() const;

    // Where the origin of the pane lies in the picture. Nothing for a pane that is not shown.
    std::optional<Formats::Vector2> PlaceOf(std::string_view paneName) const;

    // From where to where the rectangle of the pane reaches across the picture. Nothing for a pane that is not shown.
    std::optional<HorizontalSpan> SpanOf(std::string_view paneName) const;

    // True when the place of the picture lies on the rectangle of the pane. False for a pane that is not shown.
    bool IsOnPane(std::string_view paneName, Formats::Vector2 place) const;

private:
    const Layout::PanePlacement* Find(std::string_view paneName) const;

    MenuLayout& layout;
    std::vector<Layout::PanePlacement> placements;
};

using PaneFilter = std::function<bool(const Formats::Pane&)>;

// Draws the placed panes that the filter lets through. The other panes are not drawn but still give the places of the ones that are.
void DrawPanes(MenuLayout& layout, Display& display, const PlacedPanes& placedPanes, const PaneFilter& isDrawn);

// The root pane of the layout lies at this place of the picture. Every layout of the menu calls its root pane RootPane.
void MoveLayoutTo(MenuLayout& layout, Formats::Vector2 place);

// A picture shows the first texture of another picture, as the digits of the clock do.
void CopyFirstTexture(MenuLayout& layout, std::string_view toPictureName, std::string_view fromPictureName);

// The animations of a layout play as their file says: once, or again and again.
Layout::PlaybackMode ModeOfAnimationFile(const Layout::LayoutArchive& archive, std::string_view animationName);

// A group of the first level below the root group, as the menu looks groups up. Nothing when the layout has none of that name.
const Formats::Group* FindTopGroup(const Formats::Layout& file, std::string_view groupName);

}
