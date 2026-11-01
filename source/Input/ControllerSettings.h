#pragma once

#include "Input/GamepadRemote.h"
#include "Input/StickPointer.h"

#include <vector>

namespace Input
{

// What moves the hand of a Joy-Con: turning it (its gyroscope) or its stick.
enum class JoyConPointer
{
    Gyroscope,
    Stick,
};

// How the controllers that are not Wii Remotes play a Wii Remote: the page Controls of PC Settings changes these.
struct ControllerSettings
{
    std::vector<GamepadBinding> gamepadMap = {DefaultGamepadMap.begin(), DefaultGamepadMap.end()};
    PointerStick gamepadPointerStick = PointerStick::Left;
    JoyConPointer joyConPointer = JoyConPointer::Gyroscope;

    // For the sticks and the gyroscopes alike.
    int pointerSpeed = UsualPointerSpeed;

    bool operator==(const ControllerSettings&) const = default;
};

}
