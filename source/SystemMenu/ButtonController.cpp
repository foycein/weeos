#include "SystemMenu/ButtonController.h"

#include "SystemMenu/MenuError.h"

#include <algorithm>
#include <format>

namespace SystemMenu
{

ButtonController::ButtonController(MenuLayout& menuLayout, const Display& menuDisplay)
    : layout(menuLayout)
    , display(menuDisplay)
{
}

void ButtonController::AddButton(const std::string& paneName)
{
    Button button;
    button.name = paneName;
    button.pane = &layout.PaneNamed(paneName);
    buttons.push_back(button);
}

void ButtonController::SetEnabled(std::string_view paneName, bool isEnabled)
{
    FindButton(paneName).isEnabled = isEnabled;
}

ButtonController::Button& ButtonController::FindButton(std::string_view paneName)
{
    auto position = std::find_if(buttons.begin(), buttons.end(), [&](const Button& button) { return button.name == paneName; });
    if (position == buttons.end())
        throw MenuError(std::format("The button {} was never added to the controller", paneName));

    return *position;
}

void ButtonController::ForgetPointers()
{
    for (Button& button : buttons)
        button.playersInside.reset();
}

std::vector<ButtonEvent> ButtonController::Poll(const InputManager& input)
{
    std::vector<ButtonEvent> events;
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
        PollPlayer(playerIndex, input, events);

    return events;
}

void ButtonController::PollPlayer(int playerIndex, const InputManager& input, std::vector<ButtonEvent>& events)
{
    std::vector<const Formats::Pane*> panesUnderPointer = PanesUnderPointer(playerIndex, input);

    for (Button& button : buttons)
    {
        if (!button.isEnabled)
            continue;

        bool isPointedAt = std::find(panesUnderPointer.begin(), panesUnderPointer.end(), button.pane) != panesUnderPointer.end();
        UpdateFocus(button, playerIndex, isPointedAt, events);
    }

    if (!input.HasPlayerPressed(playerIndex, SelectButtons))
        return;

    // The pane in front gets the press. The panes are listed from the front.
    for (const Formats::Pane* pane : panesUnderPointer)
    {
        auto pressedButton = std::find_if(buttons.begin(), buttons.end(),
            [&](const Button& button) { return button.isEnabled && button.pane == pane; });
        if (pressedButton != buttons.end())
        {
            events.push_back({ButtonEventKind::Pressed, pressedButton->name, playerIndex});
            return;
        }
    }
}

std::vector<const Formats::Pane*> ButtonController::PanesUnderPointer(int playerIndex, const InputManager& input) const
{
    const std::optional<PlayerInput>& player = input.Player(playerIndex);
    if (!player || !player->state.pointerIsValid)
        return {};

    Formats::Vector2 point = display.PointerToCanvas(player->state.pointer);
    return layout.BoundingPanesAt(point);
}

void ButtonController::UpdateFocus(Button& button, int playerIndex, bool isPointedAt, std::vector<ButtonEvent>& events)
{
    bool wasPointedAt = button.playersInside[playerIndex];
    if (isPointedAt == wasPointedAt)
        return;

    button.playersInside[playerIndex] = isPointedAt;

    if (isPointedAt && button.playersInside.count() == 1)
        events.push_back({ButtonEventKind::FocusGained, button.name, playerIndex});

    if (!isPointedAt && button.playersInside.none())
        events.push_back({ButtonEventKind::FocusLost, button.name, playerIndex});
}

}
