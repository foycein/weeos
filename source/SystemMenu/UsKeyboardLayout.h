#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace SystemMenu
{

// What one key of the grid of the soft keyboard types: without Shift and with Shift.
struct KeyCharacters
{
    char16_t normal;
    char16_t shifted;
};

// The grid of the soft keyboard has 50 keys that type a character. They lie in five rows, the number of a key counts along the rows.
constexpr std::size_t CharacterKeyCount = 50;

// The characters of the 50 keys for the US keyboard. Letters are lowercase without Shift and uppercase with it.
// The original does not show the characters of its keys in any file that was read, so this table is ours.
const std::array<KeyCharacters, CharacterKeyCount>& CharacterKeysOfUsKeyboard();

// The character that a key of the grid types. Shift gives the sign above the key. The letters are capitals with Shift or with the caps lock
// (not with both), and the other keys do not mind the caps lock.
char16_t CharacterOfGridKey(int keyNumber, bool isShiftDown, bool isCapsLockOn);

// The characters of the symbol window, in the order of its pages: the signs of ASCII, then the signs and letters of Latin-1.
const std::u16string& SymbolsOfTheSymbolWindow();

// The character that a key of a USB keyboard types on a keyboard with the US layout, or 0 when the key types none.
// Caps lock turns the letters around, as Shift does, and does not touch the other keys.
char16_t CharacterOfUsbKey(std::uint8_t usbKey, bool isShiftDown, bool isCapsLockOn);

}
