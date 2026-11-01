#pragma once

#include "Ios/Keyboard/KeyboardService.h"

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace SystemMenu
{

enum class TypingCommandKind
{
    // Types the character of the command.
    Character,

    Backspace,
    Delete,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    MoveToStart,
    MoveToEnd,
    SelectAll,
    ToggleCapsLock,

    // The Enter key. A field of several lines takes it as a line feed, a field of one line as the answer OK.
    Enter,

    // The Escape key.
    Cancel,
};

// What a key of a USB keyboard asks of the text field.
struct TypingCommand
{
    TypingCommandKind kind;

    // Only for Character.
    char16_t character = 0;

    // Only for the movements: Shift was down, so the text between the old and the new place of the cursor gets selected.
    bool extendsSelection = false;
};

// Turns the keys of a USB keyboard into what they ask of the text field (the keyboard library of the original, 0x815F43C0).
//
// The keyboard service only says which keys are down. This class finds the keys that went down since the last poll,
// and repeats the key that was pressed last when it stays down: after half a second, and then every 33 milliseconds.
class PhysicalKeyboard
{
public:
    static constexpr double SecondsBeforeRepeat = 0.5;
    static constexpr double SecondsBetweenRepeats = 0.033;

    // The service must stay alive as long as this object.
    explicit PhysicalKeyboard(Ios::KeyboardService& service);

    // Takes the events that the service has and gives the commands, in the order of the keys. The time is the time of the menu in seconds.
    // The caps lock state is that of the soft keyboard: the letters of a character command follow it.
    std::vector<TypingCommand> Poll(double nowSeconds, bool isCapsLockOn);

    // Shows the state of the caps lock on the light of every keyboard.
    void ShowCapsLock(bool isCapsLockOn);

private:
    struct RepeatingKey
    {
        std::uint8_t usbKey;
        double nextRepeatSeconds;
    };

    void TakeKeysChanged(const Ios::KeyboardEvent& event, double nowSeconds, bool isCapsLockOn, std::vector<TypingCommand>& commands);
    void RepeatKey(double nowSeconds, bool isCapsLockOn, std::vector<TypingCommand>& commands);
    std::optional<TypingCommand> CommandOfKey(std::uint8_t usbKey, std::uint8_t modifiers, bool isCapsLockOn) const;

    Ios::KeyboardService& keyboardService;
    std::set<std::uint32_t> keyboardIds;
    Ios::PressedKeys previouslyPressedKeys = {};
    std::uint8_t latestModifiers = 0;
    std::optional<RepeatingKey> repeatingKey;
};

}
