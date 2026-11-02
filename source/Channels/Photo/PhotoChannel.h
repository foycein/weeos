#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/Photo/PhotoSession.h"

#include <memory>

namespace Channels
{

// The Photo Channel 1.1 (HAYA): the photos and movies of the SD card, offline.
// It starts with the title screen and the main music; every change of screen fades out and in.
class PhotoChannel : public Channel
{
public:
    explicit PhotoChannel(ChannelServices& channelServices);

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;

    const PhotoScreen& CurrentScreen() const;
    PhotoSession& Session();

private:
    void ChangeScreenWhenCovered();

    ChannelServices& services;
    PhotoSession session;
    std::unique_ptr<PhotoScreen> screen;
    std::unique_ptr<PhotoScreen> waitingScreen;
};

}
