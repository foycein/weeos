#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuLayout.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

enum class ButtonEventKind
{
    // The first pointer came onto the button.
    Focused,

    // The last pointer went away from the button.
    Unfocused,

    // A player pressed A while the pointer was on the button.
    Pressed,
};

struct ButtonEvent
{
    ButtonEventKind kind;
    std::size_t buttonIndex;

    // The player whose pointer made the event.
    int playerIndex;
};

// The buttons of one layout that the pointers of the players aim at. A button is a bounding pane of the layout.
class ButtonGroup
{
public:
    // The names of the panes are the buttons, numbered in the order of the list. The layout and the display must stay alive as long as this object.
    ButtonGroup(MenuLayout& buttonLayout, Display& menuDisplay, std::vector<std::string> buttonPaneNames);

    // Once in every step. Tells what changed since the step before. While the group is not active (for example while the picture fades)
    // no pointer is on any button.
    std::vector<ButtonEvent> Update(const InputManager& input, bool isActive);

    // Forgets what every pointer aims at, so that the next Update starts from nothing.
    void Reset();

private:
    std::optional<std::size_t> ButtonUnderPointer(const std::optional<PlayerInput>& player);
    void MoveFocus(int playerIndex, std::optional<std::size_t> newButton, std::vector<ButtonEvent>& events);

    MenuLayout& layout;
    Display& display;
    std::vector<std::string> buttonNames;

    // The button that each player's pointer is on, and how many pointers are on each button.
    std::array<std::optional<std::size_t>, PlayerCount> buttonOfPlayer;
    std::vector<int> pointerCounts;
};

}
