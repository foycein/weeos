#pragma once

#include "Input/RemoteOutputs.h"
#include "Input/RemoteState.h"

#include <cstdint>
#include <optional>

namespace Input
{

// Where the owner put the sensor bar, as the sensor bar setting of the console says.
// Real remotes need it to find the screen from the two dots of the bar.
enum class SensorBarPosition : std::uint8_t
{
    BelowScreen,
    AboveScreen,
};

// One device on one channel: the virtual remote of the mouse and keyboard, a real Wii Remote, Joy-Cons or a gamepad
// (RealControllers.h). The menu asks every device the same questions in every frame.
// All the functions are called from one thread.
class RemoteSource
{
public:
    RemoteSource() = default;
    RemoteSource(const RemoteSource&) = delete;
    RemoteSource& operator=(const RemoteSource&) = delete;
    virtual ~RemoteSource() = default;

    // What the device is right now. None while it is not connected.
    virtual DeviceKind Probe() const = 0;

    // The state of the device now. Nothing while the device is not connected or when it has sent no new data
    // since the last read. The menu forgets a device that sends nothing for 60 reads in a row and stops its motor.
    // The pressed and released buttons are the ones that changed since the state that the last read returned.
    virtual std::optional<RemoteState> ReadNewState() = 0;

    // Ends the connection. The HOME menu does this for the reconnect of its controller window ("Disconnecting...").
    // The device connects again when its user presses a button.
    virtual void Disconnect() = 0;

    virtual void SetSensorBarPosition(SensorBarPosition position) = 0;

    virtual void SetRumble(bool shouldRumble) = 0;
    virtual void SetPlayerLeds(const PlayerLeds& leds) = 0;
    virtual void PlaySpeakerSound(const SpeakerSound& sound) = 0;
};

}
