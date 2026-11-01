#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuLayout.h"

#include <bitset>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

enum class ButtonEventKind
{
    // The first pointer has moved onto the button.
    FocusGained,

    // The last pointer has moved off the button, has left the picture, or the buttons stopped being active.
    FocusLost,

    // The player of a pointer on the button has pressed A.
    Pressed,
};

struct ButtonEvent
{
    ButtonEventKind kind;
    std::string buttonName;

    // The place of the button in the order the buttons were added, starting at 0.
    std::size_t buttonIndex;

    // The player whose pointer moved onto the button, moved off it or pressed it.
    int playerIndex;
};

// The buttons of a layout (the controller of the buttons in the original, 0x8136AF8C). A button is a bounding pane of the layout.
// Every screen, dialog and window of the menu uses it. In every poll the controller checks which buttons the pointers of the four
// players point at and reports what changed. A button that is not enabled neither sees the pointers nor reports anything.
class ButtonController
{
public:
    // The layout and the display must stay alive as long as the controller.
    ButtonController(MenuLayout& layout, const Display& display);

    // The same, with these buttons added and enabled, in this order.
    ButtonController(MenuLayout& layout, const Display& display, const std::vector<std::string>& enabledPaneNames);

    // Throws a MenuError when the layout has no pane with this name. A new button is not enabled.
    void AddButton(const std::string& paneName);

    void SetEnabled(std::string_view paneName, bool isEnabled);

    // Every player counts as outside every button again, so that a pointer that stays on a button reports the focus once more.
    void ForgetPointers();

    // Looks at the pointers of the players as the input manager read them, in the order of the players. For each player the buttons
    // it left come before the button it reached, and the press comes last. While the buttons are not active (for example while the
    // picture fades, 0x8136AFE0) no pointer is on any button and nothing is pressed.
    std::vector<ButtonEvent> Poll(const InputManager& input, bool isActive = true);

private:
    struct Button
    {
        std::string name;
        std::size_t index;
        const Formats::Pane* pane;
        bool isEnabled = false;
        std::bitset<PlayerCount> playersInside;
    };

    Button& FindButton(std::string_view paneName);
    void PollPlayer(int playerIndex, const InputManager& input, bool isActive, std::vector<ButtonEvent>& events);
    std::vector<const Formats::Pane*> PanesUnderPointer(int playerIndex, const InputManager& input) const;
    bool IsPointedAt(const Button& button, const std::vector<const Formats::Pane*>& panesUnderPointer) const;
    void ReportPress(int playerIndex, const std::vector<const Formats::Pane*>& panesUnderPointer, std::vector<ButtonEvent>& events);
    ButtonEvent MakeEvent(ButtonEventKind kind, const Button& button, int playerIndex) const;

    MenuLayout& layout;
    const Display& display;
    std::vector<Button> buttons;
};

}
