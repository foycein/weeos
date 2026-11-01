#pragma once

#include "Input/RemoteSource.h"
#include "Input/StickPointer.h"

#include "Platform/GamepadReading.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Input
{

// While the gamepad button is down, the button of the remote is down.
struct GamepadBinding
{
    std::uint32_t gamepadButton;
    ButtonMask button;

    bool operator==(const GamepadBinding&) const = default;
};

// Which buttons of an Xbox gamepad press the buttons of the Wii Remote, until PC Settings says otherwise: the face buttons A and B
// are A and B, X and Y are 1 and 2, the two middle buttons are + and -, the left bumper is HOME (XInput does not report the Xbox
// button) and the cross is the cross. A choice: there is nothing to compare with.
inline constexpr auto DefaultGamepadMap = std::to_array<GamepadBinding>({
    {Platform::GamepadButtons::A, RemoteButtons::A},
    {Platform::GamepadButtons::B, RemoteButtons::B},
    {Platform::GamepadButtons::X, RemoteButtons::One},
    {Platform::GamepadButtons::Y, RemoteButtons::Two},
    {Platform::GamepadButtons::Menu, RemoteButtons::Plus},
    {Platform::GamepadButtons::View, RemoteButtons::Minus},
    {Platform::GamepadButtons::LeftBumper, RemoteButtons::Home},
    {Platform::GamepadButtons::DpadUp, RemoteButtons::Up},
    {Platform::GamepadButtons::DpadDown, RemoteButtons::Down},
    {Platform::GamepadButtons::DpadLeft, RemoteButtons::Left},
    {Platform::GamepadButtons::DpadRight, RemoteButtons::Right},
});

// The stick that moves the hand.
enum class PointerStick
{
    Left,
    Right,
};

// A gamepad of the Xbox kind as a Wii Remote: its buttons press the remote's buttons and one stick moves the hand like a mouse.
// It has no lights and no speaker; its rumble is both motors. After Disconnect it is not connected until a mapped button goes down.
class GamepadRemote final : public RemoteSource
{
public:
    GamepadRemote();

    // What the gamepad says now, once in every step, and the seconds since the step before. Nothing while the slot is empty.
    void Update(const std::optional<Platform::GamepadReading>& reading, double elapsedSeconds);

    void SetButtonMap(std::span<const GamepadBinding> newButtonMap);
    void SetPointerStick(PointerStick stick);
    void SetPointerSpeed(int speed);

    // The program gives this to the motors of the gamepad.
    bool IsRumbling() const;

    DeviceKind Probe() const override;
    std::optional<RemoteState> ReadNewState() override;
    void Disconnect() override;
    void SetSensorBarPosition(SensorBarPosition position) override;
    void SetRumble(bool shouldRumble) override;
    void SetPlayerLeds(const PlayerLeds& leds) override;
    void PlaySpeakerSound(const SpeakerSound& sound) override;

private:
    ButtonMask RemoteButtonsOf(std::uint32_t gamepadButtons) const;

    std::vector<GamepadBinding> buttonMap;
    PointerStick pointerStick = PointerStick::Left;
    StickPointer stickPointer;

    std::optional<Platform::GamepadReading> latestReading;
    bool isAsleep = false;
    bool hasNewData = false;
    ButtonMask buttonsOfLastUpdate = 0;
    ButtonMask buttonsHeldAtLastRead = 0;
    bool isRumbling = false;
};

}
