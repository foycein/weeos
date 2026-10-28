#pragma once

#include <cstdint>

namespace Input
{

// A set of buttons. Every button has its own bit.
using ButtonMask = std::uint32_t;

// The bit of each button of the Wii Remote and its Nunchuk. The Wii Remote reports its buttons in two bytes.
// These masks read them as one number with the first byte as the low half.
namespace RemoteButtons
{

constexpr ButtonMask Left = 0x0001;
constexpr ButtonMask Right = 0x0002;
constexpr ButtonMask Down = 0x0004;
constexpr ButtonMask Up = 0x0008;
constexpr ButtonMask Plus = 0x0010;
constexpr ButtonMask Two = 0x0100;
constexpr ButtonMask One = 0x0200;
constexpr ButtonMask B = 0x0400;
constexpr ButtonMask A = 0x0800;
constexpr ButtonMask Minus = 0x1000;
constexpr ButtonMask NunchukZ = 0x2000;
constexpr ButtonMask NunchukC = 0x4000;
constexpr ButtonMask Home = 0x8000;

}

// The bit of each button of the Classic Controller. The controller reports its buttons in two bytes, a bit is 0 while
// the button is pressed. These masks flip the bits, so a set bit means pressed, and read the two bytes as one number
// with the first byte as the high half. The triggers are the clicks at the end of the trigger travel.
namespace ClassicButtons
{

constexpr ButtonMask Up = 0x0001;
constexpr ButtonMask Left = 0x0002;
constexpr ButtonMask RightZ = 0x0004;
constexpr ButtonMask X = 0x0008;
constexpr ButtonMask A = 0x0010;
constexpr ButtonMask Y = 0x0020;
constexpr ButtonMask B = 0x0040;
constexpr ButtonMask LeftZ = 0x0080;
constexpr ButtonMask RightTrigger = 0x0200;
constexpr ButtonMask Plus = 0x0400;
constexpr ButtonMask Home = 0x0800;
constexpr ButtonMask Minus = 0x1000;
constexpr ButtonMask LeftTrigger = 0x2000;
constexpr ButtonMask Down = 0x4000;
constexpr ButtonMask Right = 0x8000;

}

}
