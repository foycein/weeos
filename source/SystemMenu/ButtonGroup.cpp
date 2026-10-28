#include "SystemMenu/ButtonGroup.h"

#include "Input/RemoteButtons.h"

#include <cmath>
#include <utility>

namespace SystemMenu
{

namespace
{

bool IsInsideThePicture(Formats::Vector2 pointer)
{
    return std::abs(pointer.x) <= 1 && std::abs(pointer.y) <= 1;
}

bool HasPressedA(const PlayerInput& player)
{
    if ((player.state.buttonsPressed & Input::RemoteButtons::A) != 0)
        return true;

    bool hasClassicController = player.deviceKind == Input::DeviceKind::RemoteWithClassicController;
    return hasClassicController && (player.state.classic.buttonsPressed & Input::ClassicButtons::A) != 0;
}

}

ButtonGroup::ButtonGroup(MenuLayout& buttonLayout, Display& menuDisplay, std::vector<std::string> buttonPaneNames)
    : layout(buttonLayout)
    , display(menuDisplay)
    , buttonNames(std::move(buttonPaneNames))
    , pointerCounts(buttonNames.size(), 0)
{
}

std::vector<ButtonEvent> ButtonGroup::Update(const InputManager& input, bool isActive)
{
    std::vector<ButtonEvent> events;

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = input.Player(playerIndex);
        std::optional<std::size_t> button = isActive ? ButtonUnderPointer(player) : std::nullopt;
        MoveFocus(playerIndex, button, events);

        if (button && HasPressedA(*player))
            events.push_back({ButtonEventKind::Pressed, *button, playerIndex});
    }
    return events;
}

void ButtonGroup::Reset()
{
    buttonOfPlayer = {};
    pointerCounts.assign(buttonNames.size(), 0);
}

std::optional<std::size_t> ButtonGroup::ButtonUnderPointer(const std::optional<PlayerInput>& player)
{
    if (!player || !player->state.pointerIsValid || !IsInsideThePicture(player->state.pointer))
        return std::nullopt;

    Formats::Vector2 canvasPoint = display.PointerToCanvas(player->state.pointer);
    for (const Formats::Pane* pane : layout.BoundingPanesAt(canvasPoint))
    {
        for (std::size_t buttonIndex = 0; buttonIndex < buttonNames.size(); ++buttonIndex)
        {
            if (pane->name == buttonNames[buttonIndex])
                return buttonIndex;
        }
    }
    return std::nullopt;
}

void ButtonGroup::MoveFocus(int playerIndex, std::optional<std::size_t> newButton, std::vector<ButtonEvent>& events)
{
    std::optional<std::size_t> oldButton = buttonOfPlayer[playerIndex];
    if (newButton == oldButton)
        return;

    if (oldButton)
    {
        --pointerCounts[*oldButton];
        if (pointerCounts[*oldButton] == 0)
            events.push_back({ButtonEventKind::Unfocused, *oldButton, playerIndex});
    }

    if (newButton)
    {
        ++pointerCounts[*newButton];
        if (pointerCounts[*newButton] == 1)
            events.push_back({ButtonEventKind::Focused, *newButton, playerIndex});
    }

    buttonOfPlayer[playerIndex] = newButton;
}

}
