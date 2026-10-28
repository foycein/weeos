#pragma once

namespace Platform
{

// The window tells a listener the virtual key code of Windows for every key, with two refinements that a keyboard needs:
// the shift, control and alt keys come with their side, and the Enter key of the keypad has a code of its own.
// The codes are the numbers of winuser.h; the test compares them with it.
namespace KeyCode
{

constexpr int Enter = 0x0D;
constexpr int Shift = 0x10;
constexpr int Control = 0x11;

// Windows calls the Alt key VK_MENU.
constexpr int Alt = 0x12;

constexpr int LeftShift = 0xA0;
constexpr int RightShift = 0xA1;
constexpr int LeftControl = 0xA2;
constexpr int RightControl = 0xA3;
constexpr int LeftAlt = 0xA4;
constexpr int RightAlt = 0xA5;

// Windows has no code for the Enter key of the keypad. It sends the code of the Enter key and marks the key as an extended key.
constexpr int KeypadEnter = 0x10D;

}

// The code that the window gives its listener for a key. The scan code names the place of the key on the keyboard.
// An extended key is a key that sends its scan code with the mark of the extended keyboard: the right control and alt keys
// and the Enter key of the keypad are such keys.
int ReportedKeyCode(int virtualKeyCode, unsigned int scanCode, bool isExtendedKey);

}
