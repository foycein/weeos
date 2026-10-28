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
// reads its new state, and disconnects it when it sends nothing for 60 reads in a row.
// Channel numbers outside 0 to 3 throw an InputError.
class RemoteChannels
{
public:
    // The source must stay alive while it is attached. A source that sat on the channel is replaced.
    // The new source is told the sensor bar position right away.
    void Attach(int channel, RemoteSource& source);
    void Detach(int channel);

    DeviceKind Probe(int channel) const;

    // The new state of the device on the channel, or nothing when the channel is empty or the device has no new data.
    std::optional<RemoteState> Read(int channel);

    void Disconnect(int channel);

    // The sensor bar is one for the whole console, so every device gets the position. The first position is below the screen.
    void SetSensorBarPosition(SensorBarPosition position);

    // The outputs do nothing while the channel is empty.
    void SetRumble(int channel, bool shouldRumble);
    void SetPlayerLeds(int channel, const PlayerLeds& leds);
    void PlaySpeakerSound(int channel, const SpeakerSound& sound);

private:
    RemoteSource* SourceOf(int channel) const;

    std::array<RemoteSource*, RemoteChannelCount> sources = {};
    SensorBarPosition sensorBarPosition = SensorBarPosition::BelowScreen;
};

}
