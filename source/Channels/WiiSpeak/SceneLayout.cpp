#include "Channels/WiiSpeak/SceneLayout.h"

#include "Layout/LayoutError.h"

namespace Channels
{

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

// The animations of the light and the press of a button ("<layout>_<button>_onOff" and "_down") are not played: each one names
// the panes of every button of its layout, the bounding panes too, so that the hand would lose the button it lights
// (docs/audit/WiiSpeak.md).
std::optional<std::string> SceneLayout::TakePressedButton(const SystemMenu::InputManager& input)
{
    for (const SystemMenu::ButtonEvent& event : buttons.Poll(input))
    {
        if (event.kind == SystemMenu::ButtonEventKind::Pressed)
            return event.buttonName;
    }
    return std::nullopt;
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

}
