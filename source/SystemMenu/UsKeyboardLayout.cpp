#include "SystemMenu/UsKeyboardLayout.h"

#include "Ios/Keyboard/UsbKeyCodes.h"

namespace SystemMenu
{

namespace
{

constexpr int LetterCount = 26;
constexpr int DigitCount = 10;
constexpr int DistanceBetweenLowerAndUpperCase = 'a' - 'A';

constexpr std::u16string_view DigitsWithoutShift = u"1234567890";
constexpr std::u16string_view DigitsWithShift = u"!@#$%^&*()";

struct SignKey
{
    std::uint8_t usbKey;
    char16_t normal;
    char16_t shifted;
};

constexpr std::array<SignKey, 13> SignKeys = {{
    {Ios::UsbKey::Space, u' ', u' '},
    {Ios::UsbKey::Minus, u'-', u'_'},
    {Ios::UsbKey::Equals, u'=', u'+'},
    {Ios::UsbKey::LeftBracket, u'[', u'{'},
    {Ios::UsbKey::RightBracket, u']', u'}'},
    {Ios::UsbKey::Backslash, u'\\', u'|'},
    {Ios::UsbKey::IsoBackslash, u'\\', u'|'},
    {Ios::UsbKey::Semicolon, u';', u':'},
    {Ios::UsbKey::Quote, u'\'', u'"'},
    {Ios::UsbKey::Grave, u'`', u'~'},
    {Ios::UsbKey::Comma, u',', u'<'},
    {Ios::UsbKey::Period, u'.', u'>'},
    {Ios::UsbKey::Slash, u'/', u'?'},
}};

// The keys of the keypad type the same with and without Shift.
constexpr std::array<SignKey, 5> KeypadSignKeys = {{
    {Ios::UsbKey::KeypadDivide, u'/', u'/'},
    {Ios::UsbKey::KeypadMultiply, u'*', u'*'},
    {Ios::UsbKey::KeypadSubtract, u'-', u'-'},
    {Ios::UsbKey::KeypadAdd, u'+', u'+'},
    {Ios::UsbKey::KeypadDecimal, u'.', u'.'},
}};

char16_t CharacterOfSignKey(std::uint8_t usbKey, bool isShiftDown)
{
    for (const SignKey& signKey : SignKeys)
    {
        if (signKey.usbKey == usbKey)
            return isShiftDown ? signKey.shifted : signKey.normal;
    }

    for (const SignKey& signKey : KeypadSignKeys)
    {
        if (signKey.usbKey == usbKey)
            return signKey.normal;
    }
    return 0;
}

}

const std::array<KeyCharacters, CharacterKeyCount>& CharacterKeysOfUsKeyboard()
{
    static const std::array<KeyCharacters, CharacterKeyCount> keys = {{
        // The row of digits, 11 keys.
        {u'1', u'!'}, {u'2', u'@'}, {u'3', u'#'}, {u'4', u'$'}, {u'5', u'%'}, {u'6', u'^'},
        {u'7', u'&'}, {u'8', u'*'}, {u'9', u'('}, {u'0', u')'}, {u'-', u'_'},

        // The row of q, 10 keys.
        {u'q', u'Q'}, {u'w', u'W'}, {u'e', u'E'}, {u'r', u'R'}, {u't', u'T'},
        {u'y', u'Y'}, {u'u', u'U'}, {u'i', u'I'}, {u'o', u'O'}, {u'p', u'P'},

        // The row of a, 10 keys.
        {u'a', u'A'}, {u's', u'S'}, {u'd', u'D'}, {u'f', u'F'}, {u'g', u'G'},
        {u'h', u'H'}, {u'j', u'J'}, {u'k', u'K'}, {u'l', u'L'}, {u';', u':'},

        // The row of z, 10 keys.
        {u'z', u'Z'}, {u'x', u'X'}, {u'c', u'C'}, {u'v', u'V'}, {u'b', u'B'},
        {u'n', u'N'}, {u'm', u'M'}, {u',', u'<'}, {u'.', u'>'}, {u'=', u'+'},

        // The row of the space key: five keys to the left of it, four to the right.
        {u'[', u'{'}, {u']', u'}'}, {u'\\', u'|'}, {u'`', u'~'}, {u'@', u'@'},
        {u'\'', u'"'}, {u'/', u'?'}, {u'_', u'_'}, {u':', u':'},
    }};
    return keys;
}

char16_t CharacterOfGridKey(int keyNumber, bool isShiftDown, bool isCapsLockOn)
{
    const KeyCharacters& key = CharacterKeysOfUsKeyboard().at(keyNumber);

    bool isLetter = key.normal >= u'a' && key.normal <= u'z';
    if (isLetter)
        return (isShiftDown != isCapsLockOn) ? key.shifted : key.normal;

    return isShiftDown ? key.shifted : key.normal;
}

const std::u16string& SymbolsOfTheSymbolWindow()
{
    static const std::u16string symbols = [] {
        std::u16string list;
        for (char16_t character = u'!'; character <= u'~'; ++character)
        {
            bool isLetterOrDigit = (character >= u'0' && character <= u'9') || (character >= u'A' && character <= u'Z') || (character >= u'a' && character <= u'z');
            if (!isLetterOrDigit)
                list.push_back(character);
        }

        for (char16_t character = 0x00A1; character <= 0x00FF; ++character)
            list.push_back(character);

        return list;
    }();
    return symbols;
}

char16_t CharacterOfUsbKey(std::uint8_t usbKey, bool isShiftDown, bool isCapsLockOn)
{
    bool isLetter = usbKey >= Ios::UsbKey::LetterA && usbKey < Ios::UsbKey::LetterA + LetterCount;
    if (isLetter)
    {
        char16_t lowercase = static_cast<char16_t>(u'a' + (usbKey - Ios::UsbKey::LetterA));
        bool isUppercase = isShiftDown != isCapsLockOn;
        return isUppercase ? static_cast<char16_t>(lowercase - DistanceBetweenLowerAndUpperCase) : lowercase;
    }

    bool isDigit = usbKey >= Ios::UsbKey::Digit1 && usbKey < Ios::UsbKey::Digit1 + DigitCount;
    if (isDigit)
    {
        std::size_t digitIndex = usbKey - Ios::UsbKey::Digit1;
        return isShiftDown ? DigitsWithShift[digitIndex] : DigitsWithoutShift[digitIndex];
    }

    bool isKeypadDigit = usbKey >= Ios::UsbKey::Keypad1 && usbKey < Ios::UsbKey::Keypad1 + DigitCount;
    if (isKeypadDigit)
        return DigitsWithoutShift[usbKey - Ios::UsbKey::Keypad1];

    return CharacterOfSignKey(usbKey, isShiftDown);
}

}
