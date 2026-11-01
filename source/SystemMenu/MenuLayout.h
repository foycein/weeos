#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/MenuAnimation.h"

#include "Layout/AnimationPlayer.h"
#include "Layout/LayoutArchive.h"
#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// A layout as the screens of the menu use it (the MenuLayout of the original). It finds panes by name, binds animations to groups,
// starts them and asks whether they still play, and it stretches the layout for the shape of the screen.
class MenuLayout
{
public:
    // The pictures come from the archive. The display and the archive must outlive the layout.
    MenuLayout(Display& display, const Layout::LayoutArchive& archive, const std::string& layoutName);

    // The pictures come from another source, for example one that holds pictures of WeeOS's own.
    MenuLayout(Display& display, const Layout::LayoutArchive& archive, const std::string& layoutName, const Layout::PictureSource& pictures);

    // The first pane with this name, anywhere in the tree. Throws a MenuError when the layout has none.
    Formats::Pane& PaneNamed(std::string_view paneName);

    // Binds the animation "<animationName>.brlan" of the archive to the panes of the group and the panes below them.
    // Gives back the slot of the animation: 0 for the first animation that was bound, then 1 and so on.
    std::size_t BindAnimation(const std::string& animationName, std::string_view groupName, Layout::PlaybackMode mode);

    // Binds the animation to one pane and the panes below it.
    std::size_t BindAnimationToPane(const std::string& animationName, std::string_view paneName, Layout::PlaybackMode mode);

    // Binds the animation to the whole layout, as the animations of the layouts without groups are meant.
    std::size_t BindAnimationToWholeLayout(const std::string& animationName, Layout::PlaybackMode mode);

    void StartAnimation(std::size_t slot);

    // Plays a part of the animation, from one frame to the other. Many animations of the menu lie one after the other in one file.
    void StartAnimationBetween(std::size_t slot, float firstFrame, float lastFrame);

    // Shows one frame of the animation and does not play it.
    void ShowAnimationFrame(std::size_t slot, float frame);

    bool IsAnimationPlaying(std::size_t slot) const;

    // Puts the text into the text box with this name. Throws a MenuError when the pane is not a text box.
    void SetText(std::string_view paneName, std::u16string_view text);

    // The size that the text of the text box takes up, in layout units. Nothing is measured while the display has no fonts.
    Formats::Vector2 MeasureText(std::string_view paneName);

    // Lets a picture show the first texture of another picture, for example a digit of the clock.
    void CopyTexture(std::string_view toPictureName, std::string_view fromPictureName);

    // The root pane of the layout lies at this place of the picture, in layout units. The layout is at the middle of the picture at first.
    void MoveTo(Formats::Vector2 position);

    // The place of the picture where the pane lies, in layout units. Only for panes that are shown.
    Formats::Vector2 PlaceOf(std::string_view paneName);

    // The shown panes that cover a place of the picture, the one that is drawn last first.
    std::vector<const Formats::Pane*> PanesAt(Formats::Vector2 place);

    // One step: fits the layout to the shape of the screen and moves every animation that plays one frame on.
    void Calculate();

    void Draw();

    // Draws only the panes that the function accepts. The other panes still count for the places and the sizes of the ones that are drawn.
    void Draw(const std::function<bool(const Formats::Pane&)>& isDrawn);

private:
    void FitToScreenShape();
    Layout::LiveMaterial& MaterialOfPicture(std::string_view pictureName);

    Display& display;
    const Layout::LayoutArchive& archive;
    std::string layoutName;
    Layout::LiveLayout layout;
    Layout::PlacementSettings placementSettings;
    std::vector<std::unique_ptr<MenuAnimation>> animations;
};

}
