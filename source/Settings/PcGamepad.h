#pragma once

#include "Input/ControllerSettings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace Settings
{

// A button of an Xbox gamepad that the page "Gamepad Buttons" offers, with the name the page shows.
struct GamepadButtonName
{
    std::u16string_view name;
    std::uint32_t gamepadButton;
};

// The value of "None": no gamepad button presses the button of the remote.
constexpr std::uint32_t NoGamepadButton = 0;

// The gamepad buttons in the order the rows of the page go through them. The first one is "None".
std::span<const GamepadButtonName> SelectableGamepadButtons();

std::optional<GamepadButtonName> FindGamepadButton(std::uint32_t gamepadButton);
std::optional<GamepadButtonName> FindGamepadButtonByName(std::u16string_view name);

// The gamepad button that presses the button of the remote, or NoGamepadButton.
std::uint32_t GamepadButtonOf(const Input::ControllerSettings& settings, Input::ButtonMask button);

// Gives the button of the remote this gamepad button, and takes it from any other button of the remote, so two never share one.
// The list stays in the order of the buttons of the remote, so that equal settings compare equal.
void SetGamepadButtonOf(Input::ControllerSettings& settings, Input::ButtonMask button, std::uint32_t gamepadButton);

// The gamepad button a row of the page gives its button: the next one of the list in the direction (+1 or -1) that no other button
// has. The list goes round.
std::uint32_t NextFreeGamepadButton(const Input::ControllerSettings& settings, Input::ButtonMask button, int direction);

}
