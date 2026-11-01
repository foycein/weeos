#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuLayout.h"

#include "Layout/LayoutArchive.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Channels
{

// One layout of the Wii Speak Channel with its buttons. A button is a bounding pane that the hand presses with A. The animations of
// the bundle are played by the part of their name after "<layout>_".
class SceneLayout
{
public:
    // The display must outlive the scene layout. The layout is fitted to the shape of the screen.
    SceneLayout(SystemMenu::Display& display, std::unique_ptr<Layout::LayoutArchive> archive, const std::string& layoutName);

    SystemMenu::MenuLayout& Layout();

    // Throws a MenuError when the layout has no bounding pane with this name.
    void AddButton(const std::string& boundPaneName);
    void SetButtonEnabled(std::string_view boundPaneName, bool isEnabled);

    // Plays an animation of the bundle once from its start, by the part of its name after "<layout>_", for example "all_inOut".
    // Nothing happens when the bundle has no such animation.
    void PlayAnimation(const std::string& animationName);

    // True while an animation that was played has not reached its last frame.
    bool IsAnimationPlaying(const std::string& animationName);

    // Shows one frame of an animation and keeps it there.
    void ShowAnimationFrame(const std::string& animationName, float frame);

    // Once in every step: the button that a player pressed with A in this step.
    std::optional<std::string> TakePressedButton(const SystemMenu::InputManager& input);

    // The step of the animations, after TakePressedButton.
    void Calculate();
    void Draw();

    // A text wider than its box is squeezed sideways to fit.
    void SetText(std::string_view paneName, std::u16string_view text);
    void SetVisible(std::string_view paneName, bool isVisible);

private:
    std::optional<std::size_t> FindOrBindAnimation(const std::string& animationName);

    std::unique_ptr<Layout::LayoutArchive> archive;
    std::string layoutName;
    SystemMenu::MenuLayout layout;
    SystemMenu::ButtonController buttons;
    std::map<std::string, std::optional<std::size_t>> animationSlots;

    struct TextWidths
    {
        float fontWidth;
        float characterSpace;
    };
    std::map<std::string, TextWidths> widthsGivenByTheLayout;
};

}
