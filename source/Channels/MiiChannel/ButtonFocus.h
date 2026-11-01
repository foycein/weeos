#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/SoundRequests.h"

#include "Formats/Vectors.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

// A button of a layout with its focus: the pane the hand points at, the pane its focus animation is bound to (with everything below it),
// the animation, and the sound when the hand comes onto it. A button with two animations is listed twice.
struct FocusedButton
{
    std::string_view buttonPane;
    std::string_view animatedPane;
    std::string_view animationName;
    std::string_view focusSound;
};

// The buttons of a layout that show the hand is on them: the focus animation of a button (its file says it loops) loops while the hand is on it (the button
// grows and its name appears) and goes back to its first frame when the hand leaves. A button that is not shown takes no focus.
class ButtonFocus
{
public:
    // The layout and the sounds must stay alive as long as this object.
    ButtonFocus(SystemMenu::MenuLayout& buttonLayout, SystemMenu::SoundRequests& soundRequests, std::span<const FocusedButton> buttons);

    // The hand, or nothing while the screen takes no input.
    void Update(std::optional<Formats::Vector2> hand);

    std::optional<std::string_view> FocusedButtonPane() const;

private:
    struct BoundButton
    {
        FocusedButton button;
        std::vector<std::size_t> animationSlots;
    };

    std::optional<std::size_t> ButtonUnderHand(std::optional<Formats::Vector2> hand);

    SystemMenu::MenuLayout& layout;
    SystemMenu::SoundRequests& sounds;
    std::vector<BoundButton> boundButtons;
    std::optional<std::size_t> focused;
};

}
