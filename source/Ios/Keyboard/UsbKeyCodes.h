#pragma once

#include <cstdint>

namespace Ios
{

// The codes of the keys of a USB keyboard. They are the usage ids of the keyboard page of the USB HID usage tables.
// A code names the place of a key, not the letter on it. The program that reads the keys decides what a code means (the layout).
namespace UsbKey
{

// Not a key. A keyboard puts this code in every place of its report when more keys are down than the report has places.
constexpr std::uint8_t RollOverError = 0x01;

// The letters A to Z have the codes LetterA to LetterA + 25.
constexpr std::uint8_t LetterA = 0x04;

// The digits 1 to 9 have the codes Digit1 to Digit1 + 8, and the digit 0 comes after them.
constexpr std::uint8_t Digit1 = 0x1E;
constexpr std::uint8_t Digit0 = 0x27;

constexpr std::uint8_t Enter = 0x28;
constexpr std::uint8_t Escape = 0x29;
constexpr std::uint8_t Backspace = 0x2A;
constexpr std::uint8_t Tab = 0x2B;
constexpr std::uint8_t Space = 0x2C;
constexpr std::uint8_t Minus = 0x2D;
constexpr std::uint8_t Equals = 0x2E;
constexpr std::uint8_t LeftBracket = 0x2F;
constexpr std::uint8_t RightBracket = 0x30;
constexpr std::uint8_t Backslash = 0x31;
constexpr std::uint8_t Semicolon = 0x33;
constexpr std::uint8_t Quote = 0x34;
constexpr std::uint8_t Grave = 0x35;
constexpr std::uint8_t Comma = 0x36;
constexpr std::uint8_t Period = 0x37;
constexpr std::uint8_t Slash = 0x38;
constexpr std::uint8_t CapsLock = 0x39;

// The function keys F1 to F12 have the codes F1 to F1 + 11.
constexpr std::uint8_t F1 = 0x3A;

constexpr std::uint8_t PrintScreen = 0x46;
constexpr std::uint8_t ScrollLock = 0x47;
constexpr std::uint8_t Pause = 0x48;
constexpr std::uint8_t Insert = 0x49;
constexpr std::uint8_t Home = 0x4A;
constexpr std::uint8_t PageUp = 0x4B;
constexpr std::uint8_t Delete = 0x4C;
constexpr std::uint8_t End = 0x4D;
constexpr std::uint8_t PageDown = 0x4E;
constexpr std::uint8_t RightArrow = 0x4F;
constexpr std::uint8_t LeftArrow = 0x50;
constexpr std::uint8_t DownArrow = 0x51;
constexpr std::uint8_t UpArrow = 0x52;
constexpr std::uint8_t NumLock = 0x53;
constexpr std::uint8_t KeypadDivide = 0x54;
constexpr std::uint8_t KeypadMultiply = 0x55;
constexpr std::uint8_t KeypadSubtract = 0x56;
constexpr std::uint8_t KeypadAdd = 0x57;
constexpr std::uint8_t KeypadEnter = 0x58;

// The keypad digits 1 to 9 have the codes Keypad1 to Keypad1 + 8, and the digit 0 comes after them.
constexpr std::uint8_t Keypad1 = 0x59;
constexpr std::uint8_t Keypad0 = 0x62;
constexpr std::uint8_t KeypadDecimal = 0x63;

// The extra key of the keyboards with the ISO shape, next to the left shift key.
constexpr std::uint8_t IsoBackslash = 0x64;

// The key with the menu symbol, next to the right control key.
constexpr std::uint8_t Application = 0x65;

}

// A keyboard reports the keys that change the meaning of other keys in one byte, with a bit for each of them.
namespace UsbModifier
{

constexpr std::uint8_t LeftControl = 0x01;
constexpr std::uint8_t LeftShift = 0x02;
constexpr std::uint8_t LeftAlt = 0x04;
constexpr std::uint8_t LeftWindows = 0x08;
constexpr std::uint8_t RightControl = 0x10;
constexpr std::uint8_t RightShift = 0x20;
constexpr std::uint8_t RightAlt = 0x40;
constexpr std::uint8_t RightWindows = 0x80;

}

// The lights of a keyboard, one bit each, in the byte that a program writes to a keyboard. The USB HID usage tables define the first three.
namespace KeyboardLight
{

constexpr std::uint8_t NumLock = 0x01;
constexpr std::uint8_t CapsLock = 0x02;
constexpr std::uint8_t ScrollLock = 0x04;

}

}
