#include "Channels/MiiChannel/ButtonFocus.h"

namespace Channels
{

namespace
{

constexpr float FirstFrame = 0.0f;

}

ButtonFocus::ButtonFocus(SystemMenu::MenuLayout& buttonLayout, SystemMenu::SoundRequests& soundRequests,
    std::span<const FocusedButton> buttons)
    : layout(buttonLayout)
    , sounds(soundRequests)
{
    for (const FocusedButton& button : buttons)
    {
        std::string animationName(button.animationName);
        std::size_t slot = layout.BindAnimationToPane(animationName, button.animatedPane, Layout::PlaybackMode::Loop);

        bool isSecondAnimationOfTheButton = !boundButtons.empty() && boundButtons.back().button.buttonPane == button.buttonPane;
        if (isSecondAnimationOfTheButton)
        {
            boundButtons.back().animationSlots.push_back(slot);
            continue;
        }
        boundButtons.push_back({button, {slot}});
    }
}

void ButtonFocus::Update(std::optional<Formats::Vector2> hand)
{
    std::optional<std::size_t> underHand = ButtonUnderHand(hand);
    if (underHand == focused)
        return;

    if (focused)
    {
        for (std::size_t slot : boundButtons[*focused].animationSlots)
        {
            layout.StopAnimation(slot);
            layout.ShowAnimationFrame(slot, FirstFrame);
        }
    }

    focused = underHand;
    if (!focused)
        return;

    for (std::size_t slot : boundButtons[*focused].animationSlots)
        layout.StartAnimation(slot);
    sounds.Request(boundButtons[*focused].button.focusSound);
}

std::optional<std::string_view> ButtonFocus::FocusedButtonPane() const
{
    if (!focused)
        return std::nullopt;

    return boundButtons[*focused].button.buttonPane;
}

std::optional<std::size_t> ButtonFocus::ButtonUnderHand(std::optional<Formats::Vector2> hand)
{
    if (!hand)
        return std::nullopt;

    for (std::size_t buttonIndex = 0; buttonIndex < boundButtons.size(); ++buttonIndex)
    {
        if (layout.IsPointInPane(boundButtons[buttonIndex].button.buttonPane, *hand))
            return buttonIndex;
    }
    return std::nullopt;
}

}
