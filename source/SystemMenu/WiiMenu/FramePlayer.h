#pragma once

#include "SystemMenu/MenuLayout.h"

#include <cstddef>

namespace SystemMenu
{

// Plays a part of a bound animation, from one frame to another. Many animation files of the Wii Menu hold several animations
// one after the other: the strip of the channel grid slides left in frames 0 to 20 of its file and right in frames 40 to 60.
class FramePlayer
{
public:
    // The layout must outlive the player. The slot is the one that MenuLayout::BindAnimation gave back.
    FramePlayer(MenuLayout& layout, std::size_t animationSlot);

    // Shows the first frame at once and moves one frame on in every Step, up to the last frame, which stays.
    void PlayOnce(float firstFrame, float lastFrame);

    // The same, but after the last frame the first one comes again.
    void Loop(float firstFrame, float lastFrame);

    // Shows one frame and stops.
    void Show(float frame);

    bool IsPlaying() const;
    float Frame() const;

    void Step();

private:
    void Start(float firstFrame, float lastFrame, bool shouldLoop);

    MenuLayout& layout;
    std::size_t slot;
    float first = 0;
    float last = 0;
    float frame = 0;
    bool isLooping = false;
    bool isPlaying = false;
};

}
