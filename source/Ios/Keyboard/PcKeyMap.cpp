#include "Ios/Keyboard/PcKeyMap.h"

#include "Ios/Keyboard/UsbKeyCodes.h"

#include "Platform/KeyCodes.h"

namespace Ios
{

namespace
{

// The virtual key codes of Windows, as winuser.h has them. The Ios part does not include the Windows headers, so it names the codes itself.
// The test of this table compares every one of them with winuser.h.
namespace WindowsKey
{

constexpr int Backspace = 0x08;
constexpr int Tab = 0x09;
constexpr int Enter = 0x0D;
constexpr int Shift = 0x10;
constexpr int Control = 0x11;
constexpr int Alt = 0x12;
constexpr int Pause = 0x13;
constexpr int CapsLock = 0x14;
constexpr int Escape = 0x1B;
constexpr int Space = 0x20;
constexpr int PageUp = 0x21;
constexpr int PageDown = 0x22;
constexpr int End = 0x23;
constexpr int Home = 0x24;
constexpr int LeftArrow = 0x25;
constexpr int UpArrow = 0x26;
constexpr int RightArrow = 0x27;
constexpr int DownArrow = 0x28;
constexpr int PrintScreen = 0x2C;
constexpr int Insert = 0x2D;
constexpr int Delete = 0x2E;

// The digits and the letters have the codes of their characters.
constexpr int Digit0 = '0';
constexpr int Digit1 = '1';
constexpr int LetterA = 'A';

constexpr int LeftWindows = 0x5B;
constexpr int RightWindows = 0x5C;
constexpr int Application = 0x5D;
constexpr int Numpad0 = 0x60;
constexpr int Numpad1 = 0x61;
constexpr int NumpadMultiply = 0x6A;
constexpr int NumpadAdd = 0x6B;
constexpr int NumpadSubtract = 0x6D;
constexpr int NumpadDecimal = 0x6E;
constexpr int NumpadDivide = 0x6F;
constexpr int F1 = 0x70;
constexpr int NumLock = 0x90;
constexpr int ScrollLock = 0x91;
constexpr int LeftShift = 0xA0;
constexpr int RightShift = 0xA1;
constexpr int LeftControl = 0xA2;
constexpr int RightControl = 0xA3;
constexpr int LeftAlt = 0xA4;
constexpr int RightAlt = 0xA5;

// The keys with signs. Windows calls them OEM keys, because the sign on a key depends on the keyboard. These are the signs of the US keyboard.
constexpr int Semicolon = 0xBA;
constexpr int Equals = 0xBB;
constexpr int Comma = 0xBC;
constexpr int Minus = 0xBD;
constexpr int Period = 0xBE;
constexpr int Slash = 0xBF;
constexpr int Grave = 0xC0;
constexpr int LeftBracket = 0xDB;
constexpr int Backslash = 0xDC;
constexpr int RightBracket = 0xDD;
constexpr int Quote = 0xDE;
constexpr int IsoBackslash = 0xE2;

}

constexpr int LetterCount = 26;
constexpr int DigitCountWithoutZero = 9;
constexpr int FunctionKeyCount = 12;
constexpr int NumpadDigitCountWithoutZero = 9;

struct KeyPair
{
    int virtualKeyCode;
    std::uint8_t usbKey;
};

// Keys with a row of their own. The keys that come in rows follow in KeyRanges.
constexpr KeyPair KeyPairs[] =
{
    {WindowsKey::Digit0, UsbKey::Digit0},
    {WindowsKey::Enter, UsbKey::Enter},
    {WindowsKey::Escape, UsbKey::Escape},
    {WindowsKey::Backspace, UsbKey::Backspace},
    {WindowsKey::Tab, UsbKey::Tab},
    {WindowsKey::Space, UsbKey::Space},
    {WindowsKey::Minus, UsbKey::Minus},
    {WindowsKey::Equals, UsbKey::Equals},
    {WindowsKey::LeftBracket, UsbKey::LeftBracket},
    {WindowsKey::RightBracket, UsbKey::RightBracket},
    {WindowsKey::Backslash, UsbKey::Backslash},
    {WindowsKey::Semicolon, UsbKey::Semicolon},
    {WindowsKey::Quote, UsbKey::Quote},
    {WindowsKey::Grave, UsbKey::Grave},
    {WindowsKey::Comma, UsbKey::Comma},
    {WindowsKey::Period, UsbKey::Period},
    {WindowsKey::Slash, UsbKey::Slash},
    {WindowsKey::CapsLock, UsbKey::CapsLock},
    {WindowsKey::PrintScreen, UsbKey::PrintScreen},
    {WindowsKey::ScrollLock, UsbKey::ScrollLock},
    {WindowsKey::Pause, UsbKey::Pause},
    {WindowsKey::Insert, UsbKey::Insert},
    {WindowsKey::Home, UsbKey::Home},
    {WindowsKey::PageUp, UsbKey::PageUp},
    {WindowsKey::Delete, UsbKey::Delete},
    {WindowsKey::End, UsbKey::End},
    {WindowsKey::PageDown, UsbKey::PageDown},
    {WindowsKey::RightArrow, UsbKey::RightArrow},
    {WindowsKey::LeftArrow, UsbKey::LeftArrow},
    {WindowsKey::DownArrow, UsbKey::DownArrow},
    {WindowsKey::UpArrow, UsbKey::UpArrow},
    {WindowsKey::NumLock, UsbKey::NumLock},
    {WindowsKey::NumpadDivide, UsbKey::KeypadDivide},
    {WindowsKey::NumpadMultiply, UsbKey::KeypadMultiply},
    {WindowsKey::NumpadSubtract, UsbKey::KeypadSubtract},
    {WindowsKey::NumpadAdd, UsbKey::KeypadAdd},
    {Platform::KeyCode::KeypadEnter, UsbKey::KeypadEnter},
    {WindowsKey::Numpad0, UsbKey::Keypad0},
    {WindowsKey::NumpadDecimal, UsbKey::KeypadDecimal},
    {WindowsKey::IsoBackslash, UsbKey::IsoBackslash},
    {WindowsKey::Application, UsbKey::Application},
};

// Keys that come in a row, in the same order on both sides.
struct KeyRange
{
    int firstVirtualKeyCode;
    std::uint8_t firstUsbKey;
    int keyCount;
};

constexpr KeyRange KeyRanges[] =
{
    {WindowsKey::LetterA, UsbKey::LetterA, LetterCount},
    {WindowsKey::Digit1, UsbKey::Digit1, DigitCountWithoutZero},
    {WindowsKey::F1, UsbKey::F1, FunctionKeyCount},
    {WindowsKey::Numpad1, UsbKey::Keypad1, NumpadDigitCountWithoutZero},
};

struct ModifierPair
{
    int virtualKeyCode;
    std::uint8_t modifierBit;
};

constexpr ModifierPair ModifierPairs[] =
{
    {WindowsKey::LeftControl, UsbModifier::LeftControl},
    {WindowsKey::Control, UsbModifier::LeftControl},
    {WindowsKey::LeftShift, UsbModifier::LeftShift},
    {WindowsKey::Shift, UsbModifier::LeftShift},
    {WindowsKey::LeftAlt, UsbModifier::LeftAlt},
    {WindowsKey::Alt, UsbModifier::LeftAlt},
    {WindowsKey::LeftWindows, UsbModifier::LeftWindows},
    {WindowsKey::RightControl, UsbModifier::RightControl},
    {WindowsKey::RightShift, UsbModifier::RightShift},
    {WindowsKey::RightAlt, UsbModifier::RightAlt},
    {WindowsKey::RightWindows, UsbModifier::RightWindows},
};

}

std::optional<std::uint8_t> UsbKeyOfVirtualKey(int virtualKeyCode)
{
    for (const KeyPair& pair : KeyPairs)
    {
        if (pair.virtualKeyCode == virtualKeyCode)
            return pair.usbKey;
    }

    for (const KeyRange& range : KeyRanges)
    {
        int positionInRange = virtualKeyCode - range.firstVirtualKeyCode;
        bool isInRange = positionInRange >= 0 && positionInRange < range.keyCount;
        if (isInRange)
            return static_cast<std::uint8_t>(range.firstUsbKey + positionInRange);
    }

    return std::nullopt;
}

std::optional<std::uint8_t> UsbModifierOfVirtualKey(int virtualKeyCode)
{
    for (const ModifierPair& pair : ModifierPairs)
    {
        if (pair.virtualKeyCode == virtualKeyCode)
            return pair.modifierBit;
    }

    return std::nullopt;
}

}
