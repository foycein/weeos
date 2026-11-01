#pragma once

#include "Layout/LiveLayout.h"

#include "Formats/Layout.h"
#include "Formats/LayoutAnimation.h"

#include <vector>

namespace Layout
{

// The caller chooses the mode. The loop bit of an animation file does not tell: the System Menu files carry it
// also on animations that play once (pushed, focus out, fade).
enum class PlaybackMode
{
    // Plays the frames once and stops after the last one. The panes keep the values of the last frame.
    PlayOnce,

    // Starts again with the first frame after the last one.
    Loop,
};

// The panes an animation reaches: the panes given to the player and all the panes below them, or the given panes alone (an animation
// file whose pat1 section says not to bind below its groups, as the Wii Speak Channel's files do).
enum class PaneReach
{
    PanesAndThoseBelow,
    GivenPanesOnly,
};

// Plays an animation on the panes and the materials of a live layout.
//
// The animation changes the panes and materials it names. It is bound to the panes given to the player (all panes of a group,
// for example), to all the panes below them unless the reach says otherwise, and to the materials that those panes use. A name that none of these panes has,
// and no material of these panes has, is left out.
//
// The animation has as many frames as its frame count says: the frame numbers go from 0 up to one below the frame count.
// The player starts at frame 0 and shows it at once. Each step shows the next frame.
// The layout must stay alive as long as the player does, and so must the panes.
class AnimationPlayer
{
public:
    // Throws a LayoutError when the animation changes something that does not exist, for example a property number
    // that no pane has or a texture of its list that is not there.
    AnimationPlayer(LiveLayout& layout, Formats::LayoutAnimation animation, PlaybackMode mode, const std::vector<Formats::Pane*>& panes,
        PaneReach reach = PaneReach::PanesAndThoseBelow);

    // One frame further. The layout of the console does exactly this once for every picture it draws.
    void Step();

    // A part of a frame or several frames further. The values between two frames follow the curves of the animation.
    // A display that is faster than the console can show every part of the way.
    void Advance(float frameCount);

    // The frame that is shown, with the part of the frame that is left after Advance.
    float CurrentFrame() const;

    // True after a PlayOnce animation has gone beyond its last frame. A finished player changes nothing anymore.
    bool IsFinished() const;

private:
    struct PaneBinding
    {
        Formats::Pane* pane;
        std::size_t objectIndex;
    };

    struct MaterialBinding
    {
        LiveMaterial* material;
        std::size_t objectIndex;
    };

    void BindObjects(const std::vector<Formats::Pane*>& panes, PaneReach reach);
    void ApplyCurrentFrame();

    LiveLayout& layout;
    Formats::LayoutAnimation animation;
    PlaybackMode mode;
    std::vector<PaneBinding> paneBindings;
    std::vector<MaterialBinding> materialBindings;
    float frame = 0;
    bool isFinished = false;
};

}
