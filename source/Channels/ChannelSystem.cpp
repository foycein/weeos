#include "Channels/ChannelSystem.h"

#include "Channels/ChannelError.h"
#include "Channels/NativeChannels.h"

#include <format>

namespace Channels
{

ChannelSystem::ChannelSystem(Graphics::GraphicsDevice& device, SystemMenu::TitleContents& contents, Ios::RemoteService& remoteService,
    Ios::NetworkConfigService& network, Platform::ScreenShape screenShape, Ios::TitleId titleId)
    : display(device, screenShape)
    , fader(display)
    , input(remoteService)
    , services{display, clock, fader, input, sounds, contents, network, wiiMenu}
    , channel(MakeChannel(titleId, services))
{
    if (!channel)
        throw ChannelError(std::format("WeeOS has no program for the channel {}", titleId.ToString()));
}

void ChannelSystem::Update()
{
    sounds.StartNextStep();
    clock.Advance();
    input.Poll();

    if (input.AnyPlayerPressed(HomeButtons) && channel->AllowsHomeButton())
        wiiMenu.Request();

    channel->Update();
    fader.Calculate();
    LeaveWhenAsked();
}

void ChannelSystem::Draw()
{
    display.BeginPicture();
    DrawScene();
}

void ChannelSystem::DrawScene()
{
    channel->Draw();
    fader.Draw();
}

std::optional<Ios::TitleId> ChannelSystem::TakeTitleToLaunch()
{
    return titleLaunch.Take();
}

ChannelServices& ChannelSystem::Services()
{
    return services;
}

Channel& ChannelSystem::RunningChannel()
{
    return *channel;
}

void ChannelSystem::LeaveWhenAsked()
{
    if (!wiiMenu.IsRequested() || hasAskedForMenu)
        return;

    if (fader.GetStatus() == SystemMenu::FaderStatus::Clear)
        fader.StartFadeOut();

    if (fader.GetStatus() != SystemMenu::FaderStatus::Covered)
        return;

    titleLaunch.Request(Ios::SystemMenuTitleId);
    hasAskedForMenu = true;
}

}
