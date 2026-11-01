#include "SystemMenu/WiiMenu/FramePlayer.h"

namespace SystemMenu
{

FramePlayer::FramePlayer(MenuLayout& layoutToAnimate, std::size_t animationSlot)
    : layout(layoutToAnimate)
    , slot(animationSlot)
{
}

void FramePlayer::PlayOnce(float firstFrame, float lastFrame)
{
    Start(firstFrame, lastFrame, false);
}

void FramePlayer::Loop(float firstFrame, float lastFrame)
{
    Start(firstFrame, lastFrame, true);
}

void FramePlayer::Show(float frameToShow)
{
    frame = frameToShow;
    isPlaying = false;
    layout.ShowAnimationFrame(slot, frame);
}

bool FramePlayer::IsPlaying() const
{
    return isPlaying;
}

float FramePlayer::Frame() const
{
    return frame;
}

void FramePlayer::Step()
{
    if (!isPlaying)
        return;

    frame += 1;
    if (frame >= last && isLooping)
        frame = first + (frame - last);
    else if (frame >= last)
        isPlaying = false;

    if (frame > last)
        frame = last;

    layout.ShowAnimationFrame(slot, frame);
}

void FramePlayer::Start(float firstFrame, float lastFrame, bool shouldLoop)
{
    first = firstFrame;
    last = lastFrame;
    frame = firstFrame;
    isLooping = shouldLoop;
    isPlaying = firstFrame < lastFrame;
    layout.ShowAnimationFrame(slot, frame);
}

}
