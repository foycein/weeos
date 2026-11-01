#pragma once

#include "Input/RemoteButtons.h"

#include <optional>
#include <span>
#include <string_view>

namespace Settings
{

// A key of the PC that the page "Keys" offers, with the name the page shows and the virtual key code of Windows (winuser.h).
struct PcKey
{
    std::u16string_view name;
    int virtualKeyCode;
};

// A button of the virtual Wii Remote that a key can press, with the name the page shows.
struct RemoteButtonName
{
    std::u16string_view name;
    Input::ButtonMask button;
};

// The keys that can be given to a button, in the order the arrows of the page go through them. The first one is "None".
std::span<const PcKey> SelectableKeys();

// The buttons of the remote that the page "Keys" lists, in its order.
std::span<const RemoteButtonName> AssignableButtons();

// The key with this code, or nothing when the page does not offer it.
std::optional<PcKey> FindKeyByCode(int virtualKeyCode);
std::optional<PcKey> FindKeyByName(std::u16string_view name);

// The code of "None": no key presses the button.
constexpr int NoKey = 0;

}
