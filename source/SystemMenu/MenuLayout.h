#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/MenuAnimation.h"

#include "Layout/AnimationPlayer.h"
#include "Layout/LayoutArchive.h"
#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"

#include <cstddef>
#include <memory>
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

    // The first pane with this name, anywhere in the tree. Throws a MenuError when the layout has none.
    Formats::Pane& PaneNamed(std::string_view paneName);

    // Puts the text into the text box with this name. Throws a MenuError when the layout has no such text box.
    void SetText(std::string_view paneName, std::u16string_view text);

    // Binds the animation "<animationName>.brlan" of the archive to the panes of the group and the panes below them.
    // Gives back the slot of the animation: 0 for the first animation that was bound, then 1 and so on.
    std::size_t BindAnimation(const std::string& animationName, std::string_view groupName, Layout::PlaybackMode mode);

    // The same for one pane and the panes below it, for an animation that moves a whole set of panes of which this is one.
    std::size_t BindAnimationToPane(const std::string& animationName, std::string_view paneName, Layout::PlaybackMode mode);

    void StartAnimation(std::size_t slot);
    bool IsAnimationPlaying(std::size_t slot) const;

    // The animation shows its first frame at once and stands still, as if it had just started and not moved on yet.
    void ShowFirstFrameOfAnimation(std::size_t slot);

    // The animation shows its last frame at once and stands still.
    void ShowLastFrameOfAnimation(std::size_t slot);

    // One step: fits the layout to the shape of the screen and moves every animation that plays one frame on.
    void Calculate();

    // The bounding panes (the panes named B_...) of the layout that the point lies in, the one in front first.
    // The point is in layout units of the picture: the middle is (0, 0), x grows to the right and y grows upward.
    std::vector<const Formats::Pane*> BoundingPanesAt(Formats::Vector2 point) const;

    void Draw();

private:
    void FitToScreenShape();

    Display& display;
    const Layout::LayoutArchive& archive;
    std::string layoutName;
    Layout::LiveLayout layout;
    Layout::PlacementSettings placementSettings;
    std::vector<std::unique_ptr<MenuAnimation>> animations;
};

}
