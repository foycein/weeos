#include "Channels/WiiSpeak/SceneLayout.h"

#include "Layout/LayoutError.h"

#include <algorithm>

namespace Channels
{

namespace
{

constexpr float SqueezedTextMargin = 0.99f;
constexpr std::string_view BoundPanePrefix = "B_";

// "B_nbtn_00" and "nbtn_00" are both the button nbtn_00.
std::string ButtonNameOf(std::string_view boundPaneName)
{
    if (boundPaneName.starts_with(BoundPanePrefix))
        boundPaneName.remove_prefix(BoundPanePrefix.size());
    return std::string(boundPaneName);
}

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
    lights.push_back({boundPaneName});
}

void SceneLayout::SetButtonEnabled(std::string_view boundPaneName, bool isEnabled)
{
    buttons.SetEnabled(boundPaneName, isEnabled);
    ButtonLight* light = FindLight(boundPaneName);
    if (light && !isEnabled)
        light->isLit = false;
}

void SceneLayout::PlayAnimation(const std::string& animationName)
{
    BoundAnimation* bound = FindOrBindAnimation(animationName);
    if (bound)
        bound->animation->Start();
}

bool SceneLayout::IsAnimationPlaying(const std::string& animationName)
{
    BoundAnimation* bound = FindOrBindAnimation(animationName);
    return bound && bound->animation->IsPlaying();
}

void SceneLayout::ShowAnimationFrame(const std::string& animationName, float frame)
{
    BoundAnimation* bound = FindOrBindAnimation(animationName);
    if (bound)
        bound->animation->ShowFrame(frame);
}

std::optional<std::string> SceneLayout::TakePressedButton(const SystemMenu::InputManager& input)
{
    std::optional<std::string> pressed;
    for (const SystemMenu::ButtonEvent& event : buttons.Poll(input))
    {
        ButtonLight* light = FindLight(event.buttonName);
        if (event.kind == SystemMenu::ButtonEventKind::FocusGained)
            light->isLit = true;
        else if (event.kind == SystemMenu::ButtonEventKind::FocusLost)
            light->isLit = false;
        else if (!pressed)
        {
            PlayAnimation(ButtonNameOf(event.buttonName) + "_down");
            pressed = event.buttonName;
        }
    }
    return pressed;
}

void SceneLayout::Calculate()
{
    layout.Calculate();
    MoveLights();
    for (const std::unique_ptr<BoundAnimation>& bound : boundAnimations)
        bound->animation->Step();
}

void SceneLayout::Draw()
{
    layout.Draw();
}

SceneLayout::BoundAnimation* SceneLayout::FindOrBindAnimation(const std::string& animationName)
{
    auto known = animationsByName.find(animationName);
    if (known != animationsByName.end())
        return known->second;

    BoundAnimation* bound = nullptr;
    try
    {
        boundAnimations.push_back(std::make_unique<BoundAnimation>(BindToItsGroups(archive->ReadAnimation(layoutName + "_" + animationName))));
        bound = boundAnimations.back().get();
    }
    catch (const Layout::LayoutError&)
    {
        bound = nullptr;
    }
    animationsByName[animationName] = bound;
    return bound;
}

// An animation without groups changes the whole layout.
SceneLayout::BoundAnimation SceneLayout::BindToItsGroups(Formats::LayoutAnimation animation)
{
    Layout::LiveLayout& live = layout.Live();
    std::vector<Formats::Pane*> panes;
    for (const std::string& groupName : animation.groupNames)
    {
        std::vector<Formats::Pane*> panesOfGroup = live.PanesOfGroup(groupName);
        panes.insert(panes.end(), panesOfGroup.begin(), panesOfGroup.end());
    }

    Layout::PaneReach reach = animation.reachesPanesBelowTheGroups ? Layout::PaneReach::PanesAndThoseBelow : Layout::PaneReach::GivenPanesOnly;
    if (panes.empty())
    {
        panes.push_back(&live.RootPane());
        reach = Layout::PaneReach::PanesAndThoseBelow;
    }

    Layout::PlaybackMode mode = animation.isLooping ? Layout::PlaybackMode::Loop : Layout::PlaybackMode::PlayOnce;
    float lastFrame = static_cast<float>(animation.frameCount) - 1;
    auto player = std::make_unique<SystemMenu::MenuAnimation>(live, std::move(animation), mode, panes, reach);
    return {std::move(player), lastFrame};
}

SceneLayout::ButtonLight* SceneLayout::FindLight(std::string_view boundPaneName)
{
    for (ButtonLight& light : lights)
    {
        if (light.buttonName == boundPaneName)
            return &light;
    }
    return nullptr;
}

void SceneLayout::MoveLights()
{
    for (ButtonLight& light : lights)
    {
        BoundAnimation* onOff = FindOrBindAnimation(ButtonNameOf(light.buttonName) + "_onOff");
        if (!onOff)
            continue;

        float target = light.isLit ? onOff->lastFrame : 0.0f;
        if (light.frame == target)
            continue;

        if (light.isLit)
            light.frame = std::min(light.frame + 1, target);
        else
            light.frame = std::max(light.frame - 1, target);
        onOff->animation->ShowFrame(light.frame);
    }
}

// As the channel does with every text it puts in: a text wider than its box is squeezed sideways until it fits, a little narrower
// than the box. The squeeze starts again from the width the layout gave the text.
void SceneLayout::SetText(std::string_view paneName, std::u16string_view text)
{
    layout.SetText(paneName, text);
    Formats::Pane& pane = layout.PaneNamed(paneName);
    Formats::TextBox& textBox = *pane.textBox;
    std::string name(paneName);
    if (!widthsGivenByTheLayout.contains(name))
        widthsGivenByTheLayout[name] = {textBox.fontSize.x, textBox.characterSpace};

    textBox.fontSize.x = widthsGivenByTheLayout[name].fontWidth;
    textBox.characterSpace = widthsGivenByTheLayout[name].characterSpace;

    float boxWidth = pane.width;
    float textWidth = layout.WidestLineOfText(paneName);
    if (textWidth <= boxWidth)
        return;

    float squeeze = boxWidth / textWidth;
    textBox.fontSize.x *= SqueezedTextMargin * squeeze;
    textBox.characterSpace *= squeeze;
}

void SceneLayout::ShowMadeTexture(std::string_view paneName, const std::string& textureName, Layout::LoadedTexture texture)
{
    layout.Live().ProvideTexture(textureName, texture);
    Layout::LiveMaterial& material = layout.MaterialOf(layout.PaneNamed(paneName));
    material.textureNames.at(0) = textureName;
}

void SceneLayout::SetVisible(std::string_view paneName, bool isVisible)
{
    layout.PaneNamed(paneName).isVisible = isVisible;
}


}
