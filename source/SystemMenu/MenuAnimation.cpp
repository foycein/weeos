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
    player.emplace(layout, animation, mode, panes);
    isPlaying = true;
}

void MenuAnimation::ShowLastFrame()
{
    player.emplace(layout, animation, mode, panes);
    player->Advance(static_cast<float>(animation.frameCount) - 1);
    isPlaying = false;
}

void MenuAnimation::Stop()
{
    isPlaying = false;
}

bool MenuAnimation::IsPlaying() const
{
    return isPlaying;
}

void MenuAnimation::ShowFrame(float frame)
{
    player.emplace(layout, animation, mode, panes);
    player->Advance(frame);
    isPlaying = false;
}

void MenuAnimation::Step()
{
    if (!isPlaying)
        return;

    player->Step();

    float lastFrame = static_cast<float>(animation.frameCount) - 1;
    bool hasReachedLastFrame = player->CurrentFrame() >= lastFrame;
    if (mode == Layout::PlaybackMode::PlayOnce && hasReachedLastFrame)
        isPlaying = false;
}

}
