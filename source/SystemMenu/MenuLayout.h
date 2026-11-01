#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/MenuAnimation.h"

#include "Layout/AnimationPlayer.h"
#include "Layout/LayoutArchive.h"
#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"
#include "Text/LaidOutText.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// A layout as the screens of the menu use it (the MenuLayout of the original). It finds panes by name, binds animations to groups,
// starts them and asks whether they still play, and it stretches the layout for the shape of the screen.
class MenuLayout
{
public:
    // The pictures come from the archive. The display and the archive must outlive the layout.
    MenuLayout(Display& display, const Layout::LayoutArchive& archive, const std::string& layoutName);

    // The pictures come from another source, for example one that holds pictures of WeeOS's own.
    MenuLayout(Display& display, const Layout::LayoutArchive& archive, const std::string& layoutName, const Layout::PictureSource& pictures);

    // The layout itself and how it is placed, for a screen that draws it in parts.
    Layout::LiveLayout& Live();
    const Layout::PlacementSettings& Placement() const;

    // The first pane with this name, anywhere in the tree. Throws a MenuError when the layout has none.
    Formats::Pane& PaneNamed(std::string_view paneName);

    // The same, or nothing when the layout has no such pane.
    Formats::Pane* FindPane(std::string_view paneName);

    // True when the pane is shown and the point of the picture, in layout units, lies inside its rectangle.
    bool IsPointInPane(std::string_view paneName, Formats::Vector2 point);

    // The material of a picture pane. Throws a MenuError when the pane is not a picture.
    Layout::LiveMaterial& MaterialOf(const Formats::Pane& pane);

    // Puts the text into the text box with this name. Throws a MenuError when the layout has no such text box.
    void SetText(std::string_view paneName, std::u16string_view text);

    // Binds the animation "<animationName>.brlan" of the archive to the panes of the group and the panes below them.
    // Gives back the slot of the animation: 0 for the first animation that was bound, then 1 and so on.
    std::size_t BindAnimation(const std::string& animationName, std::string_view groupName, Layout::PlaybackMode mode);

    // The same with the mode that the loop bit of the animation file says: it plays once, or it plays again and again.
    std::size_t BindAnimation(const std::string& animationName, std::string_view groupName);

    // Binds the animation to every pane of the layout.
    std::size_t BindAnimationToAllPanes(const std::string& animationName);

    // The same for one pane and the panes below it, for an animation that moves a whole set of panes of which this is one.
    std::size_t BindAnimationToPane(const std::string& animationName, std::string_view paneName, Layout::PlaybackMode mode);

    void StartAnimation(std::size_t slot);

    // The animation stops where it is. The panes keep what it did to them.
    void StopAnimation(std::size_t slot);

    bool IsAnimationPlaying(std::size_t slot) const;

    // Shows a frame of the animation and plays nothing.
    void ShowAnimationFrame(std::size_t slot, float frame);

    // The animation shows its last frame at once and stands still.
    void ShowLastFrameOfAnimation(std::size_t slot);

    // One step: fits the layout to the shape of the screen and moves every animation that plays one frame on.
    void Calculate();

    // The text of the text box as it is laid out now, in the space of the pane: the lines, and the place of every character.
    // Throws a MenuError when the layout has no such text box or the display has no fonts.
    Text::LaidOutText LayOutText(std::string_view paneName);

    // How wide the widest line of the text box is when the lines end only at the line feeds, not at the edge of the box.
    float WidestLineOfText(std::string_view paneName);

    // A point in the space of the pane as a point of the picture, in layout units. Nothing while the pane is not shown.
    std::optional<Formats::Vector2> PointOfPaneInPicture(std::string_view paneName, Formats::Vector2 point);

    // The bounding panes (the panes named B_...) of the layout that the point lies in, the one in front first.
    // The point is in layout units of the picture: the middle is (0, 0), x grows to the right and y grows upward.
    std::vector<const Formats::Pane*> BoundingPanesAt(Formats::Vector2 point) const;

    void Draw();

private:
    Layout::PlaybackMode ModeOfAnimationFile(const std::string& animationName) const;
    void FitToScreenShape();

    Display& display;
    const Layout::LayoutArchive& archive;
    std::string layoutName;
    Layout::LiveLayout layout;
    Layout::PlacementSettings placementSettings;
    std::vector<std::unique_ptr<MenuAnimation>> animations;
};

}
