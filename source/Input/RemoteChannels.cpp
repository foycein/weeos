#include "Input/RemoteChannels.h"

#include "Input/InputError.h"

#include <format>

namespace Input
{

namespace
{

std::size_t IndexOfChannel(int channel)
{
    bool channelExists = channel >= 0 && channel < RemoteChannelCount;
    if (!channelExists)
        throw InputError(std::format("There is no remote channel {}. The channels are 0 to {}", channel, RemoteChannelCount - 1));

    return static_cast<std::size_t>(channel);
}

}

void RemoteChannels::Attach(int channel, RemoteSource& source)
{
    std::size_t index = IndexOfChannel(channel);
    sources[index] = &source;
    source.SetSensorBarPosition(sensorBarPosition);
}

void RemoteChannels::Detach(int channel)
{
    sources[IndexOfChannel(channel)] = nullptr;
}

DeviceKind RemoteChannels::Probe(int channel) const
{
    RemoteSource* source = SourceOf(channel);
    if (!source)
        return DeviceKind::None;

    return source->Probe();
}

std::optional<RemoteState> RemoteChannels::Read(int channel)
{
    RemoteSource* source = SourceOf(channel);
    if (!source)
        return std::nullopt;

    return source->ReadNewState();
}

void RemoteChannels::Disconnect(int channel)
{
    RemoteSource* source = SourceOf(channel);
    if (!source)
        return;

    source->Disconnect();
}

void RemoteChannels::SetSensorBarPosition(SensorBarPosition position)
{
    sensorBarPosition = position;

    for (RemoteSource* source : sources)
    {
        if (source)
            source->SetSensorBarPosition(position);
    }
}

void RemoteChannels::SetRumble(int channel, bool shouldRumble)
{
    RemoteSource* source = SourceOf(channel);
    if (!source)
        return;

    source->SetRumble(shouldRumble);
}

void RemoteChannels::SetPlayerLeds(int channel, const PlayerLeds& leds)
{
    RemoteSource* source = SourceOf(channel);
    if (!source)
        return;

    source->SetPlayerLeds(leds);
}

void RemoteChannels::PlaySpeakerSound(int channel, const SpeakerSound& sound)
{
    RemoteSource* source = SourceOf(channel);
    if (!source)
        return;

    source->PlaySpeakerSound(sound);
}

RemoteSource* RemoteChannels::SourceOf(int channel) const
{
    return sources[IndexOfChannel(channel)];
}

}
