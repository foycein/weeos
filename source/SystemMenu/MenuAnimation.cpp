#include "SystemMenu/MenuAnimation.h"

#include <utility>

namespace SystemMenu
{

MenuAnimation::MenuAnimation(Layout::LiveLayout& layoutToAnimate, Formats::LayoutAnimation animationToPlay, Layout::PlaybackMode playbackMode,
    std::vector<Formats::Pane*> panesToAnimate)
    : layout(layoutToAnimate)
    , animation(std::move(animationToPlay))
    , mode(playbackMode)
    , panes(std::move(panesToAnimate))
{
}

void MenuAnimation::Start()
{
    range.reset();
    player.emplace(layout, animation, mode, panes);
    isPlaying = true;
}

void MenuAnimation::StartBetween(float firstFrame, float lastFrame)
{
    range = FrameRange{firstFrame, lastFrame};
    ShowFrameWithNewPlayer(firstFrame);
    isPlaying = firstFrame < lastFrame;
}

void MenuAnimation::ShowFrame(float frameToShow)
{
    range.reset();
    ShowFrameWithNewPlayer(frameToShow);
    isPlaying = false;
}

bool MenuAnimation::IsPlaying() const
{
    return isPlaying;
}

void MenuAnimation::Step()
{
    if (!isPlaying)
        return;

    if (range)
        StepFrameRange();
    else
        StepWholeAnimation();
}

// A player shows frame 0 when it is made. It moves on by the number of frames that it is given.
void MenuAnimation::ShowFrameWithNewPlayer(float frameToShow)
{
    player.emplace(layout, animation, Layout::PlaybackMode::PlayOnce, panes);
    player->Advance(frameToShow);
    frame = frameToShow;
}

void MenuAnimation::StepWholeAnimation()
{
    player->Step();

    float lastFrame = static_cast<float>(animation.frameCount) - 1;
    bool hasReachedLastFrame = player->CurrentFrame() >= lastFrame;
    if (mode == Layout::PlaybackMode::PlayOnce && hasReachedLastFrame)
        isPlaying = false;
}

void MenuAnimation::StepFrameRange()
{
    player->Step();
    frame += 1;
    if (frame < range->last)
        return;

    if (mode == Layout::PlaybackMode::PlayOnce)
    {
        isPlaying = false;
        return;
    }

    float framesBeyondTheEnd = frame - range->last;
    ShowFrameWithNewPlayer(range->first + framesBeyondTheEnd);
}

}
