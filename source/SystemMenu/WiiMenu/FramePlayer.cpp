#include "SystemMenu/WiiMenu/FramePlayer.h"

#include <utility>

namespace SystemMenu
{

FramePlayer::FramePlayer(MenuLayout& layoutToAnimate, const Layout::LayoutArchive& archive, std::string_view animationName,
    std::vector<Formats::Pane*> panesToAnimate)
    : layout(layoutToAnimate)
    , animation(archive.ReadAnimation(std::string(animationName)))
    , panes(std::move(panesToAnimate))
{
}

FramePlayer::FramePlayer(MenuLayout& layoutToAnimate, const Layout::LayoutArchive& archive, std::string_view animationName)
    : FramePlayer(layoutToAnimate, archive, animationName, {&layoutToAnimate.Live().RootPane()})
{
}

void FramePlayer::PlayOnce(float firstFrame, float lastFrame)
{
    Start(firstFrame, lastFrame, firstFrame, false);
}

void FramePlayer::Loop(float firstFrame, float lastFrame)
{
    Start(firstFrame, lastFrame, firstFrame, true);
}

void FramePlayer::LoopFrom(float firstFrame, float lastFrame, float startFrame)
{
    Start(firstFrame, lastFrame, startFrame, true);
}

void FramePlayer::PlayBackwards(float firstFrame, float lastFrame)
{
    Start(firstFrame, lastFrame, lastFrame, false);
    isBackwards = true;
}

void FramePlayer::Show(float frameToShow)
{
    frame = frameToShow;
    isPlaying = false;
    ShowCurrentFrame();
}

bool FramePlayer::IsPlaying() const
{
    return isPlaying;
}

float FramePlayer::Frame() const
{
    return frame;
}

float FramePlayer::LastFrameOfTheFile() const
{
    return static_cast<float>(animation.frameCount) - 1;
}

void FramePlayer::Step()
{
    if (!isPlaying)
        return;

    if (isBackwards)
    {
        StepBackwards();
        return;
    }

    frame += 1;
    if (frame >= last && isLooping)
    {
        frame = first + (frame - last);
        ShowCurrentFrame();
        return;
    }

    if (frame >= last)
    {
        isPlaying = false;
        frame = last;
        ShowCurrentFrame();
        return;
    }

    player->Advance(1);
}

void FramePlayer::StepBackwards()
{
    frame -= 1;
    if (frame <= first)
    {
        frame = first;
        isPlaying = false;
    }
    ShowCurrentFrame();
}

void FramePlayer::Start(float firstFrame, float lastFrame, float startFrame, bool shouldLoop)
{
    isBackwards = false;
    first = firstFrame;
    last = lastFrame;
    frame = startFrame;
    isLooping = shouldLoop;
    isPlaying = firstFrame < lastFrame;
    ShowCurrentFrame();
}

// A new player stands at frame 0 and jumps to the frame in one go.
void FramePlayer::ShowCurrentFrame()
{
    player.emplace(layout.Live(), animation, Layout::PlaybackMode::PlayOnce, panes);
    player->Advance(frame);
}

}
