#include "Settings/PcKeys.h"

#include <array>

namespace Settings
{

namespace
{

constexpr auto Keys = std::to_array<PcKey>({
    {u"None", NoKey},
    {u"Enter", 0x0D},
    {u"Space", 0x20},
    {u"Backspace", 0x08},
    {u"Tab", 0x09},
    {u"Escape", 0x1B},
    {u"Home", 0x24},
    {u"End", 0x23},
    {u"Page Up", 0x21},
    {u"Page Down", 0x22},
    {u"Insert", 0x2D},
    {u"Delete", 0x2E},
    {u"Left Arrow", 0x25},
    {u"Up Arrow", 0x26},
    {u"Right Arrow", 0x27},
    {u"Down Arrow", 0x28},
    {u"0", 0x30},
    {u"1", 0x31},
    {u"2", 0x32},
    {u"3", 0x33},
    {u"4", 0x34},
    {u"5", 0x35},
    {u"6", 0x36},
    {u"7", 0x37},
    {u"8", 0x38},
    {u"9", 0x39},
    {u"A", 0x41},
    {u"B", 0x42},
    {u"C", 0x43},
    {u"D", 0x44},
    {u"E", 0x45},
    {u"F", 0x46},
    {u"G", 0x47},
    {u"H", 0x48},
    {u"I", 0x49},
    {u"J", 0x4A},
    {u"K", 0x4B},
    {u"L", 0x4C},
    {u"M", 0x4D},
    {u"N", 0x4E},
    {u"O", 0x4F},
    {u"P", 0x50},
    {u"Q", 0x51},
    {u"R", 0x52},
    {u"S", 0x53},
    {u"T", 0x54},
    {u"U", 0x55},
    {u"V", 0x56},
    {u"W", 0x57},
    {u"X", 0x58},
    {u"Y", 0x59},
    {u"Z", 0x5A},
    {u"Keypad +", 0x6B},
    {u"Keypad -", 0x6D},
    {u"+ (Plus)", 0xBB},
    {u"- (Minus)", 0xBD},
    {u"F1", 0x70},
    {u"F2", 0x71},
    {u"F3", 0x72},
    {u"F4", 0x73},
});

constexpr auto Buttons = std::to_array<RemoteButtonName>({
    {u"A", Input::RemoteButtons::A},
    {u"B", Input::RemoteButtons::B},
    {u"1", Input::RemoteButtons::One},
    {u"2", Input::RemoteButtons::Two},
    {u"+", Input::RemoteButtons::Plus},
    {u"-", Input::RemoteButtons::Minus},
    {u"HOME", Input::RemoteButtons::Home},
    {u"Up", Input::RemoteButtons::Up},
    {u"Down", Input::RemoteButtons::Down},
    {u"Left", Input::RemoteButtons::Left},
    {u"Right", Input::RemoteButtons::Right},
});

}

std::span<const PcKey> SelectableKeys()
{
    return Keys;
}

std::span<const RemoteButtonName> AssignableButtons()
{
    return Buttons;
}

std::optional<PcKey> FindKeyByCode(int virtualKeyCode)
{
    for (const PcKey& key : Keys)
    {
        if (key.virtualKeyCode == virtualKeyCode)
            return key;
    }

    return std::nullopt;
}

std::optional<PcKey> FindKeyByName(std::u16string_view name)
{
    for (const PcKey& key : Keys)
    {
        if (key.name == name)
            return key;
    }

    return std::nullopt;
}

}
