#include "Channels/WiiSpeak/SceneLayout.h"

#include "Layout/LayoutError.h"

namespace Channels
{

namespace
{

constexpr std::string_view BoundPanePrefix = "B_";
constexpr std::string_view LightAnimation = "onOff";
constexpr std::string_view PressAnimation = "down";
constexpr float FirstFrame = 0.0f;

}

SceneLayout::SceneLayout(SystemMenu::Display& display, std::unique_ptr<Layout::LayoutArchive> layoutArchive, const std::string& name)
    : archive(std::move(layoutArchive))
    , layoutName(name)
    , layout(display, *archive, name)
    , buttons(layout, display)
{
}

SystemMenu::MenuLayout& SceneLayout::Layout()
{
    return layout;
}

void SceneLayout::AddButton(const std::string& boundPaneName)
{
    buttons.AddButton(boundPaneName);
    buttons.SetEnabled(boundPaneName, true);
    FindOrBindAnimation(ButtonAnimationName(boundPaneName, LightAnimation));
    FindOrBindAnimation(ButtonAnimationName(boundPaneName, PressAnimation));
}

void SceneLayout::SetButtonEnabled(std::string_view boundPaneName, bool isEnabled)
{
    buttons.SetEnabled(boundPaneName, isEnabled);
}

void SceneLayout::PlayAnimation(const std::string& animationName)
{
    std::optional<std::size_t> slot = FindOrBindAnimation(layoutName + "_" + animationName);
    if (slot)
        layout.StartAnimation(*slot);
}

bool SceneLayout::IsAnimationPlaying(const std::string& animationName)
{
    std::optional<std::size_t> slot = FindOrBindAnimation(layoutName + "_" + animationName);
    return slot && layout.IsAnimationPlaying(*slot);
}

void SceneLayout::ShowAnimationFrame(const std::string& animationName, float frame)
{
    std::optional<std::size_t> slot = FindOrBindAnimation(layoutName + "_" + animationName);
    if (slot)
        layout.ShowAnimationFrame(*slot, frame);
}

// The light comes on when the first hand is on the button and goes out when the last one has left; a press plays the press.
std::optional<std::string> SceneLayout::TakePressedButton(const SystemMenu::InputManager& input)
{
    std::optional<std::string> pressedButton;
    for (const SystemMenu::ButtonEvent& event : buttons.Poll(input))
    {
        std::optional<std::size_t> light = FindOrBindAnimation(ButtonAnimationName(event.buttonName, LightAnimation));
        if (event.kind == SystemMenu::ButtonEventKind::FocusGained && light)
            layout.StartAnimation(*light);

        if (event.kind == SystemMenu::ButtonEventKind::FocusLost && light)
            layout.ShowAnimationFrame(*light, FirstFrame);

        if (event.kind != SystemMenu::ButtonEventKind::Pressed || pressedButton)
            continue;

        std::optional<std::size_t> press = FindOrBindAnimation(ButtonAnimationName(event.buttonName, PressAnimation));
        if (press)
            layout.StartAnimation(*press);
        pressedButton = event.buttonName;
    }
    return pressedButton;
}

void SceneLayout::Calculate()
{
    layout.Calculate();
}

void SceneLayout::Draw()
{
    layout.Draw();
}

void SceneLayout::SetText(std::string_view paneName, std::u16string_view text)
{
    layout.SetText(paneName, text);
}

void SceneLayout::SetVisible(std::string_view paneName, bool isVisible)
{
    layout.PaneNamed(paneName).isVisible = isVisible;
}

// An animation is bound the first time it is asked for; one that the bundle does not have is remembered as missing.
std::optional<std::size_t> SceneLayout::FindOrBindAnimation(const std::string& animationName)
{
    auto known = animationSlots.find(animationName);
    if (known != animationSlots.end())
        return known->second;

    std::optional<std::size_t> slot;
    try
    {
        slot = layout.BindAnimationToAllPanes(animationName);
    }
    catch (const Layout::LayoutError&)
    {
        slot = std::nullopt;
    }
    animationSlots[animationName] = slot;
    return slot;
}

std::string SceneLayout::ButtonAnimationName(const std::string& boundPaneName, std::string_view kind) const
{
    std::string_view button = boundPaneName;
    if (button.starts_with(BoundPanePrefix))
        button.remove_prefix(BoundPanePrefix.size());

    return layoutName + "_" + std::string(button) + "_" + std::string(kind);
}

}
