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

    // Sets the text of a text box. Throws a MenuError when the pane is not a text box.
    void SetText(std::string_view paneName, const std::u16string& text);

    // Binds the animation "<animationName>.brlan" of the archive to the panes of the group and the panes below them.
    // Gives back the slot of the animation: 0 for the first animation that was bound, then 1 and so on.
    std::size_t BindAnimation(const std::string& animationName, std::string_view groupName, Layout::PlaybackMode mode);

    // The same with the mode that the loop bit of the animation file says: it plays once, or it plays again and again.
    std::size_t BindAnimation(const std::string& animationName, std::string_view groupName);

    // Binds the animation to every pane of the layout.
    std::size_t BindAnimationToAllPanes(const std::string& animationName);

    void StartAnimation(std::size_t slot);
    bool IsAnimationPlaying(std::size_t slot) const;

    // Shows a frame of the animation and plays nothing.
    void ShowAnimationFrame(std::size_t slot, float frame);

    // The bounding panes that lie under a place of the picture, in layout units of the picture, the one in front first.
    std::vector<const Formats::Pane*> BoundingPanesAt(Formats::Vector2 canvasPoint);

    // One step: fits the layout to the shape of the screen and moves every animation that plays one frame on.
    void Calculate();

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
