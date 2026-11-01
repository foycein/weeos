#pragma once

#include "Layout/AnimationPlayer.h"
#include "Layout/LiveLayout.h"

#include "Formats/Layout.h"
#include "Formats/LayoutAnimation.h"

#include <optional>
#include <vector>

namespace SystemMenu
{

// One animation of the HOME menu, played the way the HOME menu library plays it (0x81377C10 to 0x81377C50): every step moves it one
// frame on, from the first frame to the last one, or round and round while it loops. It is not playing any more as soon as it shows
// its last frame. A stopped animation leaves the panes as they are.
class HomeMenuAnimation
{
public:
    // The animation is bound to the given panes and the panes below them. The layout and the panes must outlive this object.
    HomeMenuAnimation(Layout::LiveLayout& layout, Formats::LayoutAnimation animation, std::vector<Formats::Pane*> panes);

    // Plays from the first frame. An animation that plays already starts again.
    void Start();

    void Stop();

    // A looping animation that stops looping plays on to its last frame and stops there.
    void SetLooping(bool shouldLoop);

    bool IsPlaying() const;

    // One frame further, if the animation is playing.
    void Step();

private:
    Layout::LiveLayout& layout;
    Formats::LayoutAnimation animation;
    std::vector<Formats::Pane*> panes;
    std::optional<Layout::AnimationPlayer> player;
    bool isPlaying = false;
    bool isLooping = false;
};

}
