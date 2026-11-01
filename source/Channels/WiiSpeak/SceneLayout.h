#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuAnimation.h"
#include "SystemMenu/MenuLayout.h"

#include "Layout/LayoutArchive.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

// One layout of the Wii Speak Channel with its buttons. A button is a bounding pane that the hand presses with A. The animations of
// the bundle are played by the part of their name after "<layout>_", each on the groups that its section pat1 names (the scene
// layout of the program, 0x800626B0). A button lights up under the hand with its animation "<button>_onOff" and plays "<button>_down"
// when it is pressed; the name of the button is its bounding pane's without "B_".
class SceneLayout
{
public:
    // The display must outlive the scene layout. The layout is fitted to the shape of the screen.
    SceneLayout(SystemMenu::Display& display, std::unique_ptr<Layout::LayoutArchive> archive, const std::string& layoutName);

    SystemMenu::MenuLayout& Layout();

    // Throws a MenuError when the layout has no bounding pane with this name.
    void AddButton(const std::string& boundPaneName);

    // A button that is not enabled goes dark.
    void SetButtonEnabled(std::string_view boundPaneName, bool isEnabled);

    // Plays an animation of the bundle once from its start, by the part of its name after "<layout>_", for example "all_inOut".
    // Nothing happens when the bundle has no such animation.
    void PlayAnimation(const std::string& animationName);

    // True while an animation that was played has not reached its last frame.
    bool IsAnimationPlaying(const std::string& animationName);

    // Shows one frame of an animation and keeps it there.
    void ShowAnimationFrame(const std::string& animationName, float frame);

    // Once in every step: the button that a player pressed with A in this step. The buttons light up and go dark here too.
    std::optional<std::string> TakePressedButton(const SystemMenu::InputManager& input);

    // The step of the animations and the lights, after TakePressedButton.
    void Calculate();
    void Draw();

    // A text wider than its box is squeezed sideways to fit.
    void SetText(std::string_view paneName, std::u16string_view text);
    void SetVisible(std::string_view paneName, bool isVisible);

private:
    struct BoundAnimation
    {
        std::unique_ptr<SystemMenu::MenuAnimation> animation;
        float lastFrame;
    };

    // The light of a button moves one frame a step towards the last frame of its onOff animation while the hand is on it, and back
    // to the first frame when the hand leaves.
    struct ButtonLight
    {
        std::string buttonName;
        float frame = 0;
        bool isLit = false;
    };

    BoundAnimation* FindOrBindAnimation(const std::string& animationName);
    BoundAnimation BindToItsGroups(Formats::LayoutAnimation animation);
    ButtonLight* FindLight(std::string_view boundPaneName);
    void MoveLights();

    std::unique_ptr<Layout::LayoutArchive> archive;
    std::string layoutName;
    SystemMenu::MenuLayout layout;
    SystemMenu::ButtonController buttons;
    std::vector<ButtonLight> lights;

    // In the order they were bound, which is the order they change the panes in a step. An animation that the bundle does not have
    // is remembered as missing.
    std::vector<std::unique_ptr<BoundAnimation>> boundAnimations;
    std::map<std::string, BoundAnimation*> animationsByName;

    struct TextWidths
    {
        float fontWidth;
        float characterSpace;
    };
    std::map<std::string, TextWidths> widthsGivenByTheLayout;
};

}
