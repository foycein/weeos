#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuLayout.h"

#include <bitset>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

enum class ButtonEventKind
{
    // The first pointer has moved onto the button.
    FocusGained,

    // The last pointer has moved off the button, or has left the picture.
    FocusLost,

    // The player of a pointer on the button has pressed A.
    Pressed,
};

struct ButtonEvent
{
    ButtonEventKind kind;
    std::string buttonName;

    // The player whose pointer moved onto the button, moved off it or pressed it.
    int playerIndex;
};

// The buttons of a layout (the controller of the buttons in the original, 0x8136AF8C). A button is a bounding pane of the layout.
// In every poll the controller checks which buttons the pointers of the four players point at and reports what changed.
// A button that is not enabled neither sees the pointers nor reports anything.
class ButtonController
{
public:
    // The layout and the display must stay alive as long as the controller.
    ButtonController(MenuLayout& layout, const Display& display);

    // Throws a MenuError when the layout has no pane with this name. A new button is not enabled.
    void AddButton(const std::string& paneName);

    void SetEnabled(std::string_view paneName, bool isEnabled);

    // Every player counts as outside every button again, so that a pointer that stays on a button reports the focus once more.
    void ForgetPointers();

    // Looks at the pointers of the players as the input manager read them, in the order of the players.
    std::vector<ButtonEvent> Poll(const InputManager& input);

private:
    struct Button
    {
        std::string name;
        const Formats::Pane* pane;
        bool isEnabled = false;
        std::bitset<PlayerCount> playersInside;
    };

    Button& FindButton(std::string_view paneName);
    void PollPlayer(int playerIndex, const InputManager& input, std::vector<ButtonEvent>& events);
    std::vector<const Formats::Pane*> PanesUnderPointer(int playerIndex, const InputManager& input) const;
    void UpdateFocus(Button& button, int playerIndex, bool isPointedAt, std::vector<ButtonEvent>& events);

    MenuLayout& layout;
    const Display& display;
    std::vector<Button> buttons;
};

}
