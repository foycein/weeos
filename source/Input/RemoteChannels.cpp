#include "Input/RemoteChannels.h"

#include "Input/InputError.h"

#include <format>

namespace Input
{

namespace
{

static_assert(RemoteChannelCount <= PlayerLedCount, "every channel has a light of its own");

std::size_t IndexOfChannel(int channel)
{
    bool channelExists = channel >= 0 && channel < RemoteChannelCount;
    if (!channelExists)
        throw InputError(std::format("There is no remote channel {}. The channels are 0 to {}", channel, RemoteChannelCount - 1));

    return static_cast<std::size_t>(channel);
}

PlayerLeds LightOfChannel(std::size_t index)
{
    PlayerLeds leds = {};
    leds[index] = true;
    return leds;
}

}

void RemoteChannels::Attach(int channel, RemoteSource& source)
{
    std::size_t index = IndexOfChannel(channel);
    sources[index] = &source;
    wasConnected[index] = false;

    source.SetSensorBarPosition(sensorBarPosition);
    NoticeConnection(index, source.Probe());
}

void RemoteChannels::Detach(int channel)
{
    std::size_t index = IndexOfChannel(channel);
    sources[index] = nullptr;
    wasConnected[index] = false;
}

DeviceKind RemoteChannels::Probe(int channel)
{
    std::size_t index = IndexOfChannel(channel);
    RemoteSource* source = sources[index];
    if (!source)
        return DeviceKind::None;

    DeviceKind deviceKind = source->Probe();
    NoticeConnection(index, deviceKind);
    return deviceKind;
}

std::optional<RemoteState> RemoteChannels::Read(int channel)
{
    std::size_t index = IndexOfChannel(channel);
    RemoteSource* source = sources[index];
    if (!source)
        return std::nullopt;

    std::optional<RemoteState> state = source->ReadNewState();

    // A virtual remote connects again while it is read, when a button is pressed.
    NoticeConnection(index, source->Probe());
    return state;
}

void RemoteChannels::Disconnect(int channel)
{
    std::size_t index = IndexOfChannel(channel);
    RemoteSource* source = sources[index];
    if (!source)
        return;

    source->SetPlayerLeds(PlayerLeds{});
    source->Disconnect();
    NoticeConnection(index, source->Probe());
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

void RemoteChannels::SetRumbleEnabled(bool isEnabled)
{
    isRumbleEnabled = isEnabled;
}

bool RemoteChannels::IsRumbleEnabled() const
{
    return isRumbleEnabled;
}

void RemoteChannels::SetRumble(int channel, bool shouldRumble)
{
    RemoteSource* source = SourceOf(channel);
    if (!source)
        return;

    if (shouldRumble && !isRumbleEnabled)
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

void RemoteChannels::NoticeConnection(std::size_t index, DeviceKind deviceKind)
{
    bool isConnected = deviceKind != DeviceKind::None;
    bool hasJustConnected = isConnected && !wasConnected[index];
    wasConnected[index] = isConnected;

    if (hasJustConnected)
        sources[index]->SetPlayerLeds(LightOfChannel(index));
}

}
