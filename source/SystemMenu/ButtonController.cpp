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

ButtonController::ButtonController(MenuLayout& menuLayout, const Display& menuDisplay, const std::vector<std::string>& enabledPaneNames)
    : ButtonController(menuLayout, menuDisplay)
{
    for (const std::string& paneName : enabledPaneNames)
    {
        AddButton(paneName);
        SetEnabled(paneName, true);
    }
}

void ButtonController::AddButton(const std::string& paneName)
{
    Button button;
    button.name = paneName;
    button.index = buttons.size();
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

std::vector<ButtonEvent> ButtonController::Poll(const InputManager& input, bool isActive)
{
    std::vector<ButtonEvent> events;
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
        PollPlayer(playerIndex, input, isActive, events);

    return events;
}

void ButtonController::PollPlayer(int playerIndex, const InputManager& input, bool isActive, std::vector<ButtonEvent>& events)
{
    std::vector<const Formats::Pane*> panesUnderPointer;
    if (isActive)
        panesUnderPointer = PanesUnderPointer(playerIndex, input);

    for (Button& button : buttons)
    {
        bool hasLeft = button.isEnabled && button.playersInside[playerIndex] && !IsPointedAt(button, panesUnderPointer);
        if (!hasLeft)
            continue;

        button.playersInside[playerIndex] = false;
        if (button.playersInside.none())
            events.push_back(MakeEvent(ButtonEventKind::FocusLost, button, playerIndex));
    }

    for (Button& button : buttons)
    {
        bool hasArrived = button.isEnabled && !button.playersInside[playerIndex] && IsPointedAt(button, panesUnderPointer);
        if (!hasArrived)
            continue;

        button.playersInside[playerIndex] = true;
        if (button.playersInside.count() == 1)
            events.push_back(MakeEvent(ButtonEventKind::FocusGained, button, playerIndex));
    }

    if (isActive && input.HasPlayerPressed(playerIndex, SelectButtons))
        ReportPress(playerIndex, panesUnderPointer, events);
}

std::vector<const Formats::Pane*> ButtonController::PanesUnderPointer(int playerIndex, const InputManager& input) const
{
    const std::optional<PlayerInput>& player = input.Player(playerIndex);
    if (!player || !player->state.pointerIsValid)
        return {};

    Formats::Vector2 point = display.PointerToCanvas(player->state.pointer);
    return layout.BoundingPanesAt(point);
}

bool ButtonController::IsPointedAt(const Button& button, const std::vector<const Formats::Pane*>& panesUnderPointer) const
{
    return std::find(panesUnderPointer.begin(), panesUnderPointer.end(), button.pane) != panesUnderPointer.end();
}

// The pane in front gets the press. The panes are listed from the front.
void ButtonController::ReportPress(int playerIndex, const std::vector<const Formats::Pane*>& panesUnderPointer,
    std::vector<ButtonEvent>& events)
{
    for (const Formats::Pane* pane : panesUnderPointer)
    {
        auto pressedButton = std::find_if(buttons.begin(), buttons.end(),
            [&](const Button& button) { return button.isEnabled && button.pane == pane; });
        if (pressedButton != buttons.end())
        {
            events.push_back(MakeEvent(ButtonEventKind::Pressed, *pressedButton, playerIndex));
            return;
        }
    }
}

ButtonEvent ButtonController::MakeEvent(ButtonEventKind kind, const Button& button, int playerIndex) const
{
    return {kind, button.name, button.index, playerIndex};
}

}
