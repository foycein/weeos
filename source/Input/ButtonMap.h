#pragma once

#include "Input/RemoteButtons.h"

#include "Platform/WindowListener.h"

#include <array>

namespace Input
{

// The numbers Windows gives to the keys, the virtual key codes of winuser.h.
namespace VirtualKey
{

constexpr int Escape = 0x1B;
constexpr int Home = 0x24;
constexpr int LeftArrow = 0x25;
constexpr int UpArrow = 0x26;
constexpr int RightArrow = 0x27;
constexpr int DownArrow = 0x28;
constexpr int Digit1 = 0x31;
constexpr int Digit2 = 0x32;
constexpr int NumpadAdd = 0x6B;
constexpr int NumpadSubtract = 0x6D;

// The keys with the plus sign and the minus sign in the row of digits (VK_OEM_PLUS and VK_OEM_MINUS).
constexpr int PlusSign = 0xBB;
constexpr int MinusSign = 0xBD;

}

// While the key is down, the button of the virtual remote is down.
struct KeyBinding
{
    int virtualKeyCode;
    ButtonMask button;
};

// While the mouse button is down, the button of the virtual remote is down.
struct MouseBinding
{
    Platform::MouseButton mouseButton;
    ButtonMask button;
};

// Which keys and mouse buttons press the buttons of virtual remote 1. Several keys may press the same button.
// The virtual remote starts with these tables and can be given others, for example from the settings.
inline constexpr auto DefaultKeyMap = std::to_array<KeyBinding>({
    {VirtualKey::LeftArrow, RemoteButtons::Left},
    {VirtualKey::RightArrow, RemoteButtons::Right},
    {VirtualKey::DownArrow, RemoteButtons::Down},
    {VirtualKey::UpArrow, RemoteButtons::Up},
    {VirtualKey::Digit1, RemoteButtons::One},
    {VirtualKey::Digit2, RemoteButtons::Two},
    {VirtualKey::PlusSign, RemoteButtons::Plus},
    {VirtualKey::NumpadAdd, RemoteButtons::Plus},
    {VirtualKey::MinusSign, RemoteButtons::Minus},
    {VirtualKey::NumpadSubtract, RemoteButtons::Minus},
    {VirtualKey::Home, RemoteButtons::Home},
    {VirtualKey::Escape, RemoteButtons::B},
});

// A notch of the mouse wheel away from the user presses Plus, a notch towards the user presses Minus (docs/DECISIONS.md, PC controls).
constexpr ButtonMask WheelAwayButton = RemoteButtons::Plus;
constexpr ButtonMask WheelTowardsButton = RemoteButtons::Minus;

inline constexpr auto DefaultMouseMap = std::to_array<MouseBinding>({
    {Platform::MouseButton::Left, RemoteButtons::A},
    {Platform::MouseButton::Right, RemoteButtons::B},
});

}
