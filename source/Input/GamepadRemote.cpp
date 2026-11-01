#include "Input/GamepadRemote.h"

namespace Input
{

GamepadRemote::GamepadRemote()
    : buttonMap(DefaultGamepadMap.begin(), DefaultGamepadMap.end())
{
}

void GamepadRemote::Update(const std::optional<Platform::GamepadReading>& reading, double elapsedSeconds)
{
    latestReading = reading;
    if (!reading)
    {
        isAsleep = false;
        hasNewData = false;
        buttonsOfLastUpdate = 0;
        return;
    }

    ButtonMask buttons = RemoteButtonsOf(reading->buttons);
    ButtonMask newlyPressed = buttons & ~buttonsOfLastUpdate;
    buttonsOfLastUpdate = buttons;
    if (newlyPressed != 0)
        isAsleep = false;

    Formats::Vector2 leftStick = {reading->leftStickX, reading->leftStickY};
    Formats::Vector2 rightStick = {reading->rightStickX, reading->rightStickY};
    stickPointer.Move(pointerStick == PointerStick::Left ? leftStick : rightStick, elapsedSeconds);
    hasNewData = true;
}

void GamepadRemote::SetButtonMap(std::span<const GamepadBinding> newButtonMap)
{
    buttonMap.assign(newButtonMap.begin(), newButtonMap.end());
}

void GamepadRemote::SetPointerStick(PointerStick stick)
{
    pointerStick = stick;
}

void GamepadRemote::SetPointerSpeed(int speed)
{
    stickPointer.SetSpeed(speed);
}

bool GamepadRemote::IsRumbling() const
{
    return isRumbling;
}

DeviceKind GamepadRemote::Probe() const
{
    if (!latestReading || isAsleep)
        return DeviceKind::None;

    return DeviceKind::Remote;
}

std::optional<RemoteState> GamepadRemote::ReadNewState()
{
    if (Probe() == DeviceKind::None || !hasNewData)
        return std::nullopt;

    hasNewData = false;

    RemoteState state;
    state.buttonsHeld = RemoteButtonsOf(latestReading->buttons);
    state.buttonsPressed = state.buttonsHeld & ~buttonsHeldAtLastRead;
    state.buttonsReleased = buttonsHeldAtLastRead & ~state.buttonsHeld;
    buttonsHeldAtLastRead = state.buttonsHeld;

    state.pointerIsValid = true;
    state.pointer = stickPointer.Pointer();
    state.batteryLevel = latestReading->batteryLevel;
    return state;
}

void GamepadRemote::Disconnect()
{
    isAsleep = true;
    isRumbling = false;
    buttonsHeldAtLastRead = 0;
}

void GamepadRemote::SetSensorBarPosition(SensorBarPosition)
{
}

void GamepadRemote::SetRumble(bool shouldRumble)
{
    isRumbling = shouldRumble;
}

void GamepadRemote::SetPlayerLeds(const PlayerLeds&)
{
}

void GamepadRemote::PlaySpeakerSound(const SpeakerSound&)
{
}

ButtonMask GamepadRemote::RemoteButtonsOf(std::uint32_t gamepadButtons) const
{
    ButtonMask buttons = 0;
    for (const GamepadBinding& binding : buttonMap)
    {
        if ((gamepadButtons & binding.gamepadButton) != 0)
            buttons |= binding.button;
    }

    return buttons;
}

}
