#include "Channels/Photo/PhotoChannel.h"

namespace Channels
{

PhotoChannel::PhotoChannel(ChannelServices& channelServices)
    : services(channelServices)
    , session(channelServices)
{
    PhotoStorage::StartResult result = session.storage.Start();
    if (result == PhotoStorage::StartResult::Ready)
        screen = MakeTitleScreen(session);
    else
        screen = MakeSaveErrorScreen(session, result);

    services.sounds.Request(PhotoSound::MainMusic);
}

void PhotoChannel::Update()
{
    SystemMenu::Fader& fader = services.fader;
    if (!waitingScreen && fader.GetStatus() == SystemMenu::FaderStatus::Covered && !services.wiiMenu.IsRequested())
        fader.StartFadeIn();

    if (waitingScreen)
    {
        ChangeScreenWhenCovered();
    }
    else if (!services.wiiMenu.IsRequested())
    {
        screen->Update();
        waitingScreen = session.TakeNextScreen();
        if (waitingScreen)
            fader.StartFadeOut();
    }
}

void PhotoChannel::Draw()
{
    screen->Draw();
}

SystemMenu::CursorManager& PhotoChannel::Hands()
{
    return session.Cursors();
}

const PhotoScreen& PhotoChannel::CurrentScreen() const
{
    return *screen;
}

PhotoSession& PhotoChannel::Session()
{
    return session;
}

void PhotoChannel::ChangeScreenWhenCovered()
{
    if (services.fader.GetStatus() != SystemMenu::FaderStatus::Covered)
        return;

    screen = std::move(waitingScreen);
    services.fader.StartFadeIn();
}

}
