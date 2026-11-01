#pragma once

#include <cstdint>

namespace Platform
{

// Windows reads up to four gamepads of the Xbox kind (XInput), in the slots 0 to 3.
constexpr int GamepadSlotCount = 4;

// A battery counts in levels from 0 (empty) to 4 (full), like the battery of a Wii Remote.
constexpr int FullBatteryLevel = 4;

// The bit of every button of an Xbox gamepad. They are the bits of XInput (xinput.h); the triggers have no bit there and get two
// bits of their own above them, set while a trigger is pulled half way or more.
namespace GamepadButtons
{

constexpr std::uint32_t DpadUp = 0x0001;
constexpr std::uint32_t DpadDown = 0x0002;
constexpr std::uint32_t DpadLeft = 0x0004;
constexpr std::uint32_t DpadRight = 0x0008;
constexpr std::uint32_t Menu = 0x0010;
constexpr std::uint32_t View = 0x0020;
constexpr std::uint32_t LeftStickClick = 0x0040;
constexpr std::uint32_t RightStickClick = 0x0080;
constexpr std::uint32_t LeftBumper = 0x0100;
constexpr std::uint32_t RightBumper = 0x0200;
constexpr std::uint32_t A = 0x1000;
constexpr std::uint32_t B = 0x2000;
constexpr std::uint32_t X = 0x4000;
constexpr std::uint32_t Y = 0x8000;
constexpr std::uint32_t LeftTrigger = 0x10000;
constexpr std::uint32_t RightTrigger = 0x20000;

}

// What a gamepad says at one moment.
struct GamepadReading
{
    // The bits of GamepadButtons that are down.
    std::uint32_t buttons = 0;

    // From -1 to 1. x grows to the right and y grows upward; (0, 0) while the stick rests in the middle.
    float leftStickX = 0;
    float leftStickY = 0;
    float rightStickX = 0;
    float rightStickY = 0;

    // A gamepad on a cable is full.
    int batteryLevel = FullBatteryLevel;
};

}
