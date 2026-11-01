#include "Ios/Remote/RemoteService.h"

namespace Ios
{

namespace
{

// The window of the program starts like this (docs/DECISIONS.md). The program tells the virtual remote the real size and shape.
constexpr Platform::WindowSize StartingWindowSize = {1280, 720};
constexpr Platform::ScreenShape StartingScreenShape = Platform::ScreenShape::SixteenByNine;

}

RemoteService::RemoteService()
    : virtualRemote(StartingWindowSize, StartingScreenShape)
{
    channels.Attach(VirtualRemoteChannel, virtualRemote);
    NoticeConnection(VirtualRemoteChannel);
}

Input::VirtualRemote& RemoteService::GetVirtualRemote()
{
    return virtualRemote;
}

void RemoteService::AttachSource(int channel, Input::RemoteSource& source)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (isClosed)
        return;

    ReleaseChannel(channel);
    channels.Attach(channel, source);
    NoticeConnection(channel);
}

void RemoteService::DetachSource(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (isClosed)
        return;

    ReleaseChannel(channel);
}

void RemoteService::SetSensorBarPosition(Input::SensorBarPosition position)
{
    std::lock_guard<std::mutex> lock(mutex);
    channels.SetSensorBarPosition(position);
}

void RemoteService::SetRumbleEnabled(bool isEnabled)
{
    std::lock_guard<std::mutex> lock(mutex);
    channels.SetRumbleEnabled(isEnabled);
}

bool RemoteService::IsRumbleEnabled()
{
    std::lock_guard<std::mutex> lock(mutex);
    return channels.IsRumbleEnabled();
}

Input::DeviceKind RemoteService::Probe(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);
    NoticeConnection(channel);
    return RecordOf(channel).deviceKind;
}

std::optional<Input::RemoteState> RemoteService::ReadNewState(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);
    NoticeConnection(channel);

    std::optional<Input::RemoteState> state = channels.Read(channel);
    if (state)
        NoticeBattery(channel, state->batteryLevel);

    return state;
}

void RemoteService::Disconnect(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);
    channels.Disconnect(channel);
    NoticeConnection(channel);
}

void RemoteService::SetRumble(int channel, bool shouldRumble)
{
    std::lock_guard<std::mutex> lock(mutex);
    channels.SetRumble(channel, shouldRumble);
}

void RemoteService::SetPlayerLeds(int channel, const Input::PlayerLeds& leds)
{
    std::lock_guard<std::mutex> lock(mutex);
    channels.SetPlayerLeds(channel, leds);
}

void RemoteService::PlaySpeakerSound(int channel, const Input::SpeakerSound& sound)
{
    std::lock_guard<std::mutex> lock(mutex);
    channels.PlaySpeakerSound(channel, sound);
}

EventQueue<RemoteEvent>& RemoteService::Events()
{
    return events;
}

void RemoteService::Close()
{
    std::lock_guard<std::mutex> lock(mutex);
    isClosed = true;

    for (int channel = 0; channel < Input::RemoteChannelCount; ++channel)
    {
        channels.Detach(channel);
        RecordOf(channel) = ChannelRecord();
    }

    events.Close();
}

RemoteService::ChannelRecord& RemoteService::RecordOf(int channel)
{
    return records[static_cast<std::size_t>(channel)];
}

// The caller holds the lock. Takes the device off the channel. A device that was connected is reported as disconnected.
void RemoteService::ReleaseChannel(int channel)
{
    channels.Detach(channel);

    ChannelRecord& record = RecordOf(channel);
    if (record.deviceKind != Input::DeviceKind::None)
        events.Post(RemoteEvent{RemoteEventKind::Disconnected, channel});

    record = ChannelRecord();
}

// The caller holds the lock. Asks the device what it is now and reports a connect or a disconnect.
// A change from one extension to another is no event: the kind that Probe gives changes.
void RemoteService::NoticeConnection(int channel)
{
    Input::DeviceKind deviceKind = channels.Probe(channel);

    ChannelRecord& record = RecordOf(channel);
    bool wasConnected = record.deviceKind != Input::DeviceKind::None;
    bool isConnected = deviceKind != Input::DeviceKind::None;
    record.deviceKind = deviceKind;

    if (!wasConnected && isConnected)
        events.Post(RemoteEvent{RemoteEventKind::Connected, channel});

    if (wasConnected && !isConnected)
    {
        record.batteryLevel.reset();
        events.Post(RemoteEvent{RemoteEventKind::Disconnected, channel});
    }
}

// The caller holds the lock. The first level after a connect is what the battery is. Every other level is compared with the one before.
void RemoteService::NoticeBattery(int channel, int batteryLevel)
{
    ChannelRecord& record = RecordOf(channel);
    bool hasChanged = record.batteryLevel.has_value() && *record.batteryLevel != batteryLevel;
    record.batteryLevel = batteryLevel;

    if (hasChanged)
        events.Post(RemoteEvent{RemoteEventKind::BatteryChanged, channel});
}

}
