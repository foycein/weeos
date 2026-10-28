#include "SystemMenu/KeyboardDigitEntry.h"

#include "Ios/Keyboard/UsbKeyCodes.h"

namespace SystemMenu
{

namespace
{

constexpr int DigitsInARow = 10;

// The digit 0 comes after the digit 9 in the codes of the keys.
std::optional<char16_t> DigitOfKey(std::uint8_t key)
{
    for (int digit = 1; digit <= DigitsInARow; ++digit)
    {
        int digitValue = digit % DigitsInARow;
        bool isTopRowKey = key == Ios::UsbKey::Digit1 + digit - 1;
        bool isKeypadKey = key == Ios::UsbKey::Keypad1 + digit - 1;
        if (isTopRowKey || isKeypadKey)
            return static_cast<char16_t>(u'0' + digitValue);
    }
    return std::nullopt;
}

}

KeyboardDigitEntry::KeyboardDigitEntry(Ios::KeyboardService& keyboardService)
    : keyboards(keyboardService)
{
}

void KeyboardDigitEntry::Begin(int playerIndex, std::size_t maximumCharacters)
{
    static_cast<void>(playerIndex);
    maximumLength = maximumCharacters;
    text.clear();
    state = TextEntryState::Typing;
}

void KeyboardDigitEntry::Update()
{
    while (std::optional<Ios::KeyboardEvent> event = keyboards.TryTakeEvent())
    {
        if (event->kind != Ios::KeyboardEventKind::KeysChanged)
            continue;

        std::set<std::uint8_t> keysNowDown;
        for (std::uint8_t key : event->pressedKeys)
        {
            keysNowDown.insert(key);
            if (state == TextEntryState::Typing && !keysDown.contains(key))
                HandleKey(key);
        }
        keysDown = keysNowDown;
    }
}

void KeyboardDigitEntry::HandleKey(std::uint8_t key)
{
    if (key == Ios::UsbKey::Enter || key == Ios::UsbKey::KeypadEnter)
    {
        state = TextEntryState::Finished;
        return;
    }

    if (key == Ios::UsbKey::Escape)
    {
        text.clear();
        state = TextEntryState::Cancelled;
        return;
    }

    if (key == Ios::UsbKey::Backspace && !text.empty())
    {
        text.pop_back();
        return;
    }

    std::optional<char16_t> digit = DigitOfKey(key);
    if (digit && text.size() < maximumLength)
        text.push_back(*digit);
}

TextEntryState KeyboardDigitEntry::State() const
{
    return state;
}

const std::u16string& KeyboardDigitEntry::Text() const
{
    return text;
}

void KeyboardDigitEntry::End()
{
    state = TextEntryState::Idle;
}

}
