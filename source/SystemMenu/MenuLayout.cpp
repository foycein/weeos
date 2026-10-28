#include "SystemMenu/MenuLayout.h"

#include "SystemMenu/MenuError.h"

#include "Layout/HitTest.h"
#include "Layout/LayoutDrawer.h"

#include <format>

namespace SystemMenu
{

MenuLayout::MenuLayout(Display& menuDisplay, const Layout::LayoutArchive& layoutArchive, const std::string& nameOfLayout)
    : MenuLayout(menuDisplay, layoutArchive, nameOfLayout, layoutArchive)
{
}

MenuLayout::MenuLayout(Display& menuDisplay, const Layout::LayoutArchive& layoutArchive, const std::string& nameOfLayout,
    const Layout::PictureSource& pictures)
    : display(menuDisplay)
    , archive(layoutArchive)
    , layoutName(nameOfLayout)
    , layout(menuDisplay.Device(), pictures, layoutArchive.ReadLayout(nameOfLayout))
{
    FitToScreenShape();
}

Formats::Pane& MenuLayout::PaneNamed(std::string_view paneName)
{
    Formats::Pane* pane = layout.FindPane(paneName);
    if (pane == nullptr)
        throw MenuError(std::format("The layout {} has no pane {}", layoutName, paneName));

    return *pane;
}

void MenuLayout::SetText(std::string_view paneName, std::u16string_view text)
{
    Formats::Pane& pane = PaneNamed(paneName);
    if (!pane.textBox)
        throw MenuError(std::format("The pane {} of the layout {} is not a text box", paneName, layoutName));

    pane.textBox->text = text;
}

std::size_t MenuLayout::BindAnimation(const std::string& animationName, std::string_view groupName, Layout::PlaybackMode mode)
{
    animations.push_back(std::make_unique<MenuAnimation>(layout, archive.ReadAnimation(animationName), mode, layout.PanesOfGroup(groupName)));
    return animations.size() - 1;
}

std::size_t MenuLayout::BindAnimationToPane(const std::string& animationName, std::string_view paneName, Layout::PlaybackMode mode)
{
    std::vector<Formats::Pane*> panes = Layout::PaneWithDescendants(PaneNamed(paneName));
    animations.push_back(std::make_unique<MenuAnimation>(layout, archive.ReadAnimation(animationName), mode, panes));
    return animations.size() - 1;
}

void MenuLayout::StartAnimation(std::size_t slot)
{
    animations.at(slot)->Start();
}

bool MenuLayout::IsAnimationPlaying(std::size_t slot) const
{
    return animations.at(slot)->IsPlaying();
}

void MenuLayout::ShowFirstFrameOfAnimation(std::size_t slot)
{
    animations.at(slot)->ShowFirstFrame();
}

void MenuLayout::ShowLastFrameOfAnimation(std::size_t slot)
{
    animations.at(slot)->ShowLastFrame();
}

void MenuLayout::Calculate()
{
    FitToScreenShape();

    for (const std::unique_ptr<MenuAnimation>& animation : animations)
        animation->Step();
}

std::vector<const Formats::Pane*> MenuLayout::BoundingPanesAt(Formats::Vector2 point) const
{
    return Layout::FindBoundingPanesAt(layout.RootPane(), point, placementSettings);
}

void MenuLayout::Draw()
{
    Layout::DrawLayout(display.Device(), layout, placementSettings, display.TextFonts());
}

// On a wide screen the root pane stretches the whole layout to fill the picture, and the panes that adjust their location are
// squeezed by the opposite so that they keep their shape. On a 4:3 screen nothing is stretched (docs/audit/OpenPoints/SystemMenu.md SYS-17).
void MenuLayout::FitToScreenShape()
{
    Formats::Vector2 stretch = display.LayoutStretch();
    layout.RootPane().scale = stretch;
    placementSettings.locationAdjustScale = {1 / stretch.x, 1 / stretch.y};
}

}
