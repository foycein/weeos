#include "SystemMenu/PhysicalKeyboard.h"

#include "SystemMenu/UsKeyboardLayout.h"

#include "Ios/Keyboard/UsbKeyCodes.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

bool IsShiftDown(std::uint8_t modifiers)
{
    return (modifiers & (Ios::UsbModifier::LeftShift | Ios::UsbModifier::RightShift)) != 0;
}

bool IsControlDown(std::uint8_t modifiers)
{
    return (modifiers & (Ios::UsbModifier::LeftControl | Ios::UsbModifier::RightControl)) != 0;
}

// The places of a report that hold no key are 0, and a report that holds the roll over error holds no real key.
bool IsRealKey(std::uint8_t usbKey)
{
    return usbKey != 0 && usbKey != Ios::UsbKey::RollOverError;
}

bool IsDown(const Ios::PressedKeys& pressedKeys, std::uint8_t usbKey)
{
    return std::find(pressedKeys.begin(), pressedKeys.end(), usbKey) != pressedKeys.end();
}

// The lock keys, Print Screen, Pause and the menu key do not repeat (0x81692590).
bool CanRepeat(std::uint8_t usbKey)
{
    constexpr std::array<std::uint8_t, 6> keysWithoutRepeat = {Ios::UsbKey::CapsLock, Ios::UsbKey::NumLock, Ios::UsbKey::ScrollLock,
        Ios::UsbKey::PrintScreen, Ios::UsbKey::Pause, Ios::UsbKey::Application};
    return std::find(keysWithoutRepeat.begin(), keysWithoutRepeat.end(), usbKey) == keysWithoutRepeat.end();
}

TypingCommand MakeCommand(TypingCommandKind kind, bool extendsSelection = false)
{
    TypingCommand command;
    command.kind = kind;
    command.extendsSelection = extendsSelection;
    return command;
}

TypingCommand MakeCharacterCommand(char16_t character)
{
    TypingCommand command;
    command.kind = TypingCommandKind::Character;
    command.character = character;
    return command;
}

}

PhysicalKeyboard::PhysicalKeyboard(Ios::KeyboardService& service)
    : keyboardService(service)
{
}

std::vector<TypingCommand> PhysicalKeyboard::Poll(double nowSeconds, bool isCapsLockOn)
{
    std::vector<TypingCommand> commands;

    while (std::optional<Ios::KeyboardEvent> event = keyboardService.TryTakeEvent())
    {
        if (event->kind == Ios::KeyboardEventKind::Connected)
        {
            keyboardIds.insert(event->keyboardId);
        }
        else if (event->kind == Ios::KeyboardEventKind::Disconnected)
        {
            keyboardIds.erase(event->keyboardId);
        }
        else
        {
            TakeKeysChanged(*event, nowSeconds, isCapsLockOn, commands);
        }
    }

    RepeatKey(nowSeconds, isCapsLockOn, commands);
    return commands;
}

void PhysicalKeyboard::TakeKeysChanged(const Ios::KeyboardEvent& event, double nowSeconds, bool isCapsLockOn,
    std::vector<TypingCommand>& commands)
{
    latestModifiers = event.modifiers;

    for (std::uint8_t usbKey : event.pressedKeys)
    {
        if (!IsRealKey(usbKey) || IsDown(previouslyPressedKeys, usbKey))
            continue;

        std::optional<TypingCommand> command = CommandOfKey(usbKey, event.modifiers, isCapsLockOn);
        if (command)
            commands.push_back(*command);

        repeatingKey.reset();
        if (CanRepeat(usbKey))
            repeatingKey = RepeatingKey{usbKey, nowSeconds + SecondsBeforeRepeat};
    }

    if (repeatingKey && !IsDown(event.pressedKeys, repeatingKey->usbKey))
        repeatingKey.reset();

    previouslyPressedKeys = event.pressedKeys;
}

void PhysicalKeyboard::RepeatKey(double nowSeconds, bool isCapsLockOn, std::vector<TypingCommand>& commands)
{
    if (!repeatingKey)
        return;

    while (nowSeconds >= repeatingKey->nextRepeatSeconds)
    {
        std::optional<TypingCommand> command = CommandOfKey(repeatingKey->usbKey, latestModifiers, isCapsLockOn);
        if (command)
            commands.push_back(*command);

        repeatingKey->nextRepeatSeconds += SecondsBetweenRepeats;
    }
}

std::optional<TypingCommand> PhysicalKeyboard::CommandOfKey(std::uint8_t usbKey, std::uint8_t modifiers, bool isCapsLockOn) const
{
    bool isShiftDown = IsShiftDown(modifiers);

    switch (usbKey)
    {
    case Ios::UsbKey::Enter:
        return MakeCommand(TypingCommandKind::Enter);
    case Ios::UsbKey::Escape:
        return MakeCommand(TypingCommandKind::Cancel);
    case Ios::UsbKey::Backspace:
        return MakeCommand(TypingCommandKind::Backspace);
    case Ios::UsbKey::Delete:
        return MakeCommand(TypingCommandKind::Delete);
    case Ios::UsbKey::LeftArrow:
        return MakeCommand(TypingCommandKind::MoveLeft, isShiftDown);
    case Ios::UsbKey::RightArrow:
        return MakeCommand(TypingCommandKind::MoveRight, isShiftDown);
    case Ios::UsbKey::UpArrow:
        return MakeCommand(TypingCommandKind::MoveUp, isShiftDown);
    case Ios::UsbKey::DownArrow:
        return MakeCommand(TypingCommandKind::MoveDown, isShiftDown);
    case Ios::UsbKey::Home:
        return MakeCommand(TypingCommandKind::MoveToStart, isShiftDown);
    case Ios::UsbKey::End:
        return MakeCommand(TypingCommandKind::MoveToEnd, isShiftDown);
    case Ios::UsbKey::CapsLock:
        return MakeCommand(TypingCommandKind::ToggleCapsLock);
    default:
        break;
    }

    if (IsControlDown(modifiers))
    {
        if (usbKey == Ios::UsbKey::LetterA)
            return MakeCommand(TypingCommandKind::SelectAll);

        return std::nullopt;
    }

    char16_t character = CharacterOfUsbKey(usbKey, isShiftDown, isCapsLockOn);
    if (character == 0)
        return std::nullopt;

    return MakeCharacterCommand(character);
}

void PhysicalKeyboard::ShowCapsLock(bool isCapsLockOn)
{
    std::uint8_t lights = isCapsLockOn ? Ios::KeyboardLight::CapsLock : 0;
    for (std::uint32_t keyboardId : keyboardIds)
        keyboardService.SetLights(keyboardId, lights);
}

}
