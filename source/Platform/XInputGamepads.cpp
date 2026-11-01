#include "Platform/InputDevices.h"

#include <windows.h>

#include <xinput.h>

#include <algorithm>

namespace Platform
{

namespace
{

constexpr float LargestStickValue = 32767.0f;

// A trigger counts as a button from half of its way on. XInput gives the trigger as 0 to 255.
constexpr BYTE TriggerPressedFrom = 128;

// Both motors at full speed while the remote rumbles.
constexpr WORD FullMotorSpeed = 65535;

float StickValue(SHORT value)
{
    return std::clamp(value / LargestStickValue, -1.0f, 1.0f);
}

int BatteryLevelOf(int slot)
{
    XINPUT_BATTERY_INFORMATION battery = {};
    if (XInputGetBatteryInformation(slot, BATTERY_DEVTYPE_GAMEPAD, &battery) != ERROR_SUCCESS)
        return FullBatteryLevel;

    if (battery.BatteryType == BATTERY_TYPE_WIRED)
        return FullBatteryLevel;

    switch (battery.BatteryLevel)
    {
    case BATTERY_LEVEL_EMPTY:
        return 0;
    case BATTERY_LEVEL_LOW:
        return 1;
    case BATTERY_LEVEL_MEDIUM:
        return FullBatteryLevel - 1;
    default:
        return FullBatteryLevel;
    }
}

}

std::optional<GamepadReading> WindowsInputDevices::ReadGamepad(int slot)
{
    XINPUT_STATE state = {};
    if (XInputGetState(slot, &state) != ERROR_SUCCESS)
        return std::nullopt;

    const XINPUT_GAMEPAD& gamepad = state.Gamepad;

    GamepadReading reading;
    reading.buttons = gamepad.wButtons;
    if (gamepad.bLeftTrigger >= TriggerPressedFrom)
        reading.buttons |= GamepadButtons::LeftTrigger;
    if (gamepad.bRightTrigger >= TriggerPressedFrom)
        reading.buttons |= GamepadButtons::RightTrigger;

    reading.leftStickX = StickValue(gamepad.sThumbLX);
    reading.leftStickY = StickValue(gamepad.sThumbLY);
    reading.rightStickX = StickValue(gamepad.sThumbRX);
    reading.rightStickY = StickValue(gamepad.sThumbRY);
    reading.batteryLevel = BatteryLevelOf(slot);
    return reading;
}

void WindowsInputDevices::SetGamepadRumble(int slot, bool shouldRumble)
{
    XINPUT_VIBRATION vibration = {};
    if (shouldRumble)
    {
        vibration.wLeftMotorSpeed = FullMotorSpeed;
        vibration.wRightMotorSpeed = FullMotorSpeed;
    }

    XInputSetState(slot, &vibration);
}

}
