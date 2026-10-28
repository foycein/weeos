#pragma once

#include "Input/RemoteSource.h"

#include <array>
#include <optional>

namespace Input
{

// The four channels of the console. A device sits on a channel as long as it is attached, and an empty channel
// answers like a remote that is not connected.
//
// In every frame the menu tells the sensor bar position and goes through the channels 0 to 3: it probes the device,
// reads its new state, and forgets a device that sends nothing for 60 reads in a row.
// Channel numbers outside 0 to 3 throw an InputError.
//
// This class does the part of the original WPAD library that does not depend on the device (OpenPoints/Input.md):
// it lights the light of the channel number when a device connects and turns the lights off before it disconnects one,
// and it drops the requests to rumble while the vibration setting is off.
class RemoteChannels
{
public:
    // The source must stay alive while it is attached. A source that sat on the channel is replaced.
    // The new source is told the sensor bar position right away, and the light of the channel when it is connected.
    void Attach(int channel, RemoteSource& source);
    void Detach(int channel);

    // A device that has connected since the last call gets the light of its channel.
    DeviceKind Probe(int channel);

    // The new state of the device on the channel, or nothing when the channel is empty or the device has no new data.
    std::optional<RemoteState> Read(int channel);

    // Turns the lights of the device off and ends its connection.
    void Disconnect(int channel);

    // The sensor bar is one for the whole console, so every device gets the position. The first position is below the screen.
    void SetSensorBarPosition(SensorBarPosition position);

    // The vibration setting of the console (BT.MOT). While it is off, a request to start the rumble is dropped.
    // A request to stop is always passed on. The first setting is on.
    void SetRumbleEnabled(bool isEnabled);
    bool IsRumbleEnabled() const;

    // The outputs do nothing while the channel is empty.
    void SetRumble(int channel, bool shouldRumble);
    void SetPlayerLeds(int channel, const PlayerLeds& leds);
    void PlaySpeakerSound(int channel, const SpeakerSound& sound);

private:
    RemoteSource* SourceOf(int channel) const;
    void NoticeConnection(std::size_t index, DeviceKind deviceKind);

    std::array<RemoteSource*, RemoteChannelCount> sources = {};
    std::array<bool, RemoteChannelCount> wasConnected = {};
    SensorBarPosition sensorBarPosition = SensorBarPosition::BelowScreen;
    bool isRumbleEnabled = true;
};

}
