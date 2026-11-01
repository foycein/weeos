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
    // The animation is bound to the given panes and the panes below them. The layout and the panes must outlive this object.
    MenuAnimation(Layout::LiveLayout& layout, Formats::LayoutAnimation animation, Layout::PlaybackMode mode, std::vector<Formats::Pane*> panes);

    // Plays from the first frame to the last. An animation that plays already starts again.
    void Start();

    // Plays the frames from the first to the last given one, as a screen plays a part of an animation file that holds many animations
    // one after the other. A loop starts again at the first of the two frames. The panes of the group show the first frame at once.
    void StartBetween(float firstFrame, float lastFrame);

    // Shows one frame and does not play.
    void ShowFrame(float frame);

    bool IsPlaying() const;

    // One frame further, if the animation is playing.
    void Step();

private:
    struct FrameRange
    {
        float first;
        float last;
    };

    void ShowFrameWithNewPlayer(float frame);
    void StepWholeAnimation();
    void StepFrameRange();

    Layout::LiveLayout& layout;
    Formats::LayoutAnimation animation;
    Layout::PlaybackMode mode;
    std::vector<Formats::Pane*> panes;
    std::optional<Layout::AnimationPlayer> player;
    std::optional<FrameRange> range;
    float frame = 0;
    bool isPlaying = false;
};

}
