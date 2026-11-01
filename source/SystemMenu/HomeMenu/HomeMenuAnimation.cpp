#include "SystemMenu/HomeMenu/HomeMenuAnimation.h"

#include <utility>

namespace SystemMenu
{

HomeMenuAnimation::HomeMenuAnimation(Layout::LiveLayout& layoutToAnimate, Formats::LayoutAnimation animationToPlay,
    std::vector<Formats::Pane*> panesToAnimate)
    : layout(layoutToAnimate)
    , animation(std::move(animationToPlay))
    , panes(std::move(panesToAnimate))
{
}

// The player always loops, so that an animation can start or stop looping while it plays. Step decides where a single round ends.
void HomeMenuAnimation::Start()
{
    player.emplace(layout, animation, Layout::PlaybackMode::Loop, panes);
    isPlaying = true;
}

void HomeMenuAnimation::Stop()
{
    isPlaying = false;
}

void HomeMenuAnimation::SetLooping(bool shouldLoop)
{
    isLooping = shouldLoop;
}

bool HomeMenuAnimation::IsPlaying() const
{
    return isPlaying;
}

void HomeMenuAnimation::Step()
{
    if (!isPlaying)
        return;

    player->Step();

    float lastFrame = static_cast<float>(animation.frameCount) - 1;
    bool hasReachedLastFrame = player->CurrentFrame() >= lastFrame;
    if (!isLooping && hasReachedLastFrame)
        isPlaying = false;
}

}
