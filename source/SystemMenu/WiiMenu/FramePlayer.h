#pragma once

#include "SystemMenu/MenuLayout.h"

#include "Formats/LayoutAnimation.h"
#include "Layout/AnimationPlayer.h"
#include "Layout/LayoutArchive.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// Plays a part of an animation file, from one frame to another. Many animation files of the Wii Menu hold several animations
// one after the other: the strip of the channel grid slides left in frames 0 to 20 of its file and right in frames 40 to 60.
class FramePlayer
{
public:
    // The animation "<animationName>.brlan" of the archive plays on the panes and the panes below them. The layout must outlive the player.
    FramePlayer(MenuLayout& layout, const Layout::LayoutArchive& archive, std::string_view animationName, std::vector<Formats::Pane*> panes);

    // The same, on every pane of the layout.
    FramePlayer(MenuLayout& layout, const Layout::LayoutArchive& archive, std::string_view animationName);

    // Shows the first frame at once and moves one frame on in every Step, up to the last frame, which stays.
    void PlayOnce(float firstFrame, float lastFrame);

    // The same, but after the last frame the first one comes again.
    void Loop(float firstFrame, float lastFrame);

    // Shows one frame and stops.
    void Show(float frame);

    bool IsPlaying() const;
    float Frame() const;

    // The last frame of the whole file.
    float LastFrameOfTheFile() const;

    void Step();

private:
    void Start(float firstFrame, float lastFrame, bool shouldLoop);
    void ShowCurrentFrame();

    MenuLayout& layout;
    Formats::LayoutAnimation animation;
    std::vector<Formats::Pane*> panes;
    std::optional<Layout::AnimationPlayer> player;
    float first = 0;
    float last = 0;
    float frame = 0;
    bool isLooping = false;
    bool isPlaying = false;
};

}
