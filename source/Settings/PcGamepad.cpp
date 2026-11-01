#include "Settings/PcGamepad.h"

#include <algorithm>
#include <array>

namespace Settings
{

namespace
{

namespace Buttons = Platform::GamepadButtons;

constexpr auto GamepadButtons = std::to_array<GamepadButtonName>({
    {u"None", NoGamepadButton},
    {u"A", Buttons::A},
    {u"B", Buttons::B},
    {u"X", Buttons::X},
    {u"Y", Buttons::Y},
    {u"Left Bumper", Buttons::LeftBumper},
    {u"Right Bumper", Buttons::RightBumper},
    {u"Left Trigger", Buttons::LeftTrigger},
    {u"Right Trigger", Buttons::RightTrigger},
    {u"View", Buttons::View},
    {u"Menu", Buttons::Menu},
    {u"Left Stick Click", Buttons::LeftStickClick},
    {u"Right Stick Click", Buttons::RightStickClick},
    {u"D-Pad Up", Buttons::DpadUp},
    {u"D-Pad Down", Buttons::DpadDown},
    {u"D-Pad Left", Buttons::DpadLeft},
    {u"D-Pad Right", Buttons::DpadRight},
});

int PositionOf(std::uint32_t gamepadButton)
{
    for (std::size_t position = 0; position < GamepadButtons.size(); ++position)
    {
        if (GamepadButtons[position].gamepadButton == gamepadButton)
            return static_cast<int>(position);
    }

    return 0;
}

bool IsTakenByAnotherButton(const Input::ControllerSettings& settings, Input::ButtonMask button, std::uint32_t gamepadButton)
{
    if (gamepadButton == NoGamepadButton)
        return false;

    for (const Input::GamepadBinding& binding : settings.gamepadMap)
    {
        if (binding.gamepadButton == gamepadButton && binding.button != button)
            return true;
    }

    return false;
}

}

std::span<const GamepadButtonName> SelectableGamepadButtons()
{
    return GamepadButtons;
}

std::optional<GamepadButtonName> FindGamepadButton(std::uint32_t gamepadButton)
{
    for (const GamepadButtonName& name : GamepadButtons)
    {
        if (name.gamepadButton == gamepadButton)
            return name;
    }

    return std::nullopt;
}

std::optional<GamepadButtonName> FindGamepadButtonByName(std::u16string_view name)
{
    for (const GamepadButtonName& button : GamepadButtons)
    {
        if (button.name == name)
            return button;
    }

    return std::nullopt;
}

std::uint32_t GamepadButtonOf(const Input::ControllerSettings& settings, Input::ButtonMask button)
{
    for (const Input::GamepadBinding& binding : settings.gamepadMap)
    {
        if (binding.button == button)
            return binding.gamepadButton;
    }

    return NoGamepadButton;
}

void SetGamepadButtonOf(Input::ControllerSettings& settings, Input::ButtonMask button, std::uint32_t gamepadButton)
{
    std::erase_if(settings.gamepadMap,
        [button, gamepadButton](const Input::GamepadBinding& binding)
        {
            bool isThisButton = binding.button == button;
            bool takesTheGamepadButton = gamepadButton != NoGamepadButton && binding.gamepadButton == gamepadButton;
            return isThisButton || takesTheGamepadButton;
        });

    if (gamepadButton == NoGamepadButton)
        return;

    settings.gamepadMap.push_back({gamepadButton, button});
    std::sort(settings.gamepadMap.begin(), settings.gamepadMap.end(),
        [](const Input::GamepadBinding& first, const Input::GamepadBinding& second) { return first.button < second.button; });
}

std::uint32_t NextFreeGamepadButton(const Input::ControllerSettings& settings, Input::ButtonMask button, int direction)
{
    int count = static_cast<int>(GamepadButtons.size());
    int position = PositionOf(GamepadButtonOf(settings, button));

    // "None" is never taken, so the search ends at the latest when it comes round to it.
    while (true)
    {
        position = (position + direction + count) % count;
        std::uint32_t gamepadButton = GamepadButtons[position].gamepadButton;
        if (!IsTakenByAnotherButton(settings, button, gamepadButton))
            return gamepadButton;
    }
}

}
