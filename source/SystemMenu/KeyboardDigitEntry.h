#pragma once

#include "SystemMenu/TextEntry.h"

#include "Ios/Keyboard/KeyboardService.h"

#include <set>

namespace SystemMenu
{

// Takes a number from the digit keys of a USB keyboard (the keyboard of the PC), through the keyboard service of IOS. The digits of the row
// above the letters and of the keypad type a digit, Backspace takes the last digit back, Enter ends the entry and Escape leaves it.
class KeyboardDigitEntry : public TextEntry
{
public:
    // The service must stay alive as long as this object.
    explicit KeyboardDigitEntry(Ios::KeyboardService& keyboardService);

    void Begin(int playerIndex, std::size_t maximumCharacters) override;
    void Update() override;
    TextEntryState State() const override;
    const std::u16string& Text() const override;
    void End() override;

private:
    void HandleKey(std::uint8_t key);

    Ios::KeyboardService& keyboards;
    TextEntryState state = TextEntryState::Idle;
    std::size_t maximumLength = 0;
    std::u16string text;

    // The keys that were down at the last event, so that only a key that goes down counts.
    std::set<std::uint8_t> keysDown;
};

}
