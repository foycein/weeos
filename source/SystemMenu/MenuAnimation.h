#pragma once

#include "Layout/AnimationPlayer.h"

#include "Formats/Layout.h"
#include "Formats/LayoutAnimation.h"

#include <optional>
#include <vector>

namespace SystemMenu
{

// One animation of a menu layout, played the way the menu plays it: it starts on request, and every step moves it one frame on.
// Unlike the layout library's player, it is not playing any more as soon as it has reached its last frame,
// which is when the menu asks whether it is still playing.
class MenuAnimation
{
public:
    // The animation is bound to the given panes and, unless the reach says otherwise, the panes below them. The layout and the panes
    // must outlive this object.
    MenuAnimation(Layout::LiveLayout& layout, Formats::LayoutAnimation animation, Layout::PlaybackMode mode, std::vector<Formats::Pane*> panes,
        Layout::PaneReach reach = Layout::PaneReach::PanesAndThoseBelow);

    // Plays from the first frame. An animation that plays already starts again.
    void Start();

    // Shows the last frame at once and stands still. The animation is not playing then.
    void ShowLastFrame();

    // The animation stops where it is. The panes keep what it did to them.
    void Stop();

    bool IsPlaying() const;

    // Shows one frame and plays nothing, for an animation that a value drives, like a progress bar.
    void ShowFrame(float frame);

    // One frame further, if the animation is playing.
    void Step();

private:
    Layout::LiveLayout& layout;
    Formats::LayoutAnimation animation;
    Layout::PlaybackMode mode;
    std::vector<Formats::Pane*> panes;
    Layout::PaneReach reach;
    std::optional<Layout::AnimationPlayer> player;
    bool isPlaying = false;
};

}
