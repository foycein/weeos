#pragma once

#include "Input/RemoteButtons.h"

#include "Formats/Vectors.h"

#include <cstdint>

namespace Input
{

// The console has four remote channels, numbered 0 to 3. Remote 1 of the menu is channel 0.
constexpr int RemoteChannelCount = 4;

// A remote counts its battery in levels from 0 (empty) to 4 (full). The HOME menu shows one bar for each level
// and warns below level 2 (OpenPoints/Input.md, INP-01).
constexpr int MaximumBatteryLevel = 4;

// The distance between the remote and the sensor bar, in meters. The pointer library works it out from the two dots of the sensor bar
// as 0.2 / tan(21 degrees) / the separation of the dots in the picture of the camera (OpenPoints/Input.md, INP-06). The menu never asks for it.
// Assumed: what a remote without a sensor bar reports. A couch is about 2 meters from the television.
constexpr float UsualDistanceInMeters = 2.0f;

enum class DeviceKind : std::uint8_t
{
    // Nothing is connected to the channel.
    None,

    Remote,
    RemoteWithNunchuk,
    RemoteWithClassicController,
};

struct NunchukState
{
    // From -1 to 1 on both axes. x grows to the right and y grows upward. It is (0, 0) while the stick rests in the middle.
    Formats::Vector2 stick = {0, 0};

    // In units of the gravity.
    Formats::Vector3 acceleration = {0, 0, 0};
};

struct ClassicControllerState
{
    // The bits are the ClassicButtons. Pressed and released say what changed since the previous state.
    ButtonMask buttonsHeld = 0;
    ButtonMask buttonsPressed = 0;
    ButtonMask buttonsReleased = 0;

    // From -1 to 1 on both axes, like the stick of the Nunchuk.
    Formats::Vector2 leftStick = {0, 0};
    Formats::Vector2 rightStick = {0, 0};

    // From 0 (not pressed) to 1 (pressed all the way).
    float leftTrigger = 0;
    float rightTrigger = 0;
};

// Everything the menu reads from one remote in one frame.
struct RemoteState
{
    // The bits are the RemoteButtons. Pressed and released say what changed since the previous state.
    ButtonMask buttonsHeld = 0;
    ButtonMask buttonsPressed = 0;
    ButtonMask buttonsReleased = 0;

    // False while the remote does not see the screen. Then the pointer means nothing and must not hit anything.
    bool pointerIsValid = false;

    // Where the remote points, in the pointer space of PointerSpace.h.
    Formats::Vector2 pointer = {0, 0};

    // How far the remote is turned around the direction it points in, in degrees. It is 0 while the remote is level.
    // This is the turn that the menu gives to the hand (the panes N_Rot and N_SRot of the cursor layout), counterclockwise
    // as seen on the screen like the rotation of a pane, so the hand can use it as it is.
    float rollDegrees = 0;

    float distanceInMeters = UsualDistanceInMeters;
    int batteryLevel = MaximumBatteryLevel;

    // Only these two are filled, and only when the device kind says that the extension is there.
    NunchukState nunchuk;
    ClassicControllerState classic;
};

}
