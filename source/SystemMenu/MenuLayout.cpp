#include "SystemMenu/MenuLayout.h"

#include "SystemMenu/MenuError.h"

#include "Layout/LayoutDrawer.h"
#include "Layout/PaneRectangle.h"

#include "Text/TextStyle.h"

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

std::size_t MenuLayout::BindAnimation(const std::string& animationName, std::string_view groupName, Layout::PlaybackMode mode)
{
    animations.push_back(std::make_unique<MenuAnimation>(layout, archive.ReadAnimation(animationName), mode, layout.PanesOfGroup(groupName)));
    return animations.size() - 1;
}

std::size_t MenuLayout::BindAnimationToPane(const std::string& animationName, std::string_view paneName, Layout::PlaybackMode mode)
{
    std::vector<Formats::Pane*> panes = {&PaneNamed(paneName)};
    animations.push_back(std::make_unique<MenuAnimation>(layout, archive.ReadAnimation(animationName), mode, panes));
    return animations.size() - 1;
}

std::size_t MenuLayout::BindAnimationToWholeLayout(const std::string& animationName, Layout::PlaybackMode mode)
{
    std::vector<Formats::Pane*> wholeLayout = {&layout.RootPane()};
    animations.push_back(std::make_unique<MenuAnimation>(layout, archive.ReadAnimation(animationName), mode, wholeLayout));
    return animations.size() - 1;
}

void MenuLayout::StartAnimation(std::size_t slot)
{
    animations.at(slot)->Start();
}

void MenuLayout::StartAnimationBetween(std::size_t slot, float firstFrame, float lastFrame)
{
    animations.at(slot)->StartBetween(firstFrame, lastFrame);
}

void MenuLayout::ShowAnimationFrame(std::size_t slot, float frame)
{
    animations.at(slot)->ShowFrame(frame);
}

bool MenuLayout::IsAnimationPlaying(std::size_t slot) const
{
    return animations.at(slot)->IsPlaying();
}

void MenuLayout::SetText(std::string_view paneName, std::u16string_view text)
{
    Formats::Pane& pane = PaneNamed(paneName);
    if (!pane.textBox)
        throw MenuError(std::format("The pane {} of the layout {} is not a text box", paneName, layoutName));

    pane.textBox->text = text;
}

Formats::Vector2 MenuLayout::MeasureText(std::string_view paneName)
{
    const Layout::TextFonts* textFonts = display.TextFonts();
    Formats::Pane& pane = PaneNamed(paneName);
    if (textFonts == nullptr || !pane.textBox)
        return {0, 0};

    const Formats::TextBox& textBox = *pane.textBox;
    const Layout::DrawableFont& font = textFonts->Find(layout.FontNameAt(textBox.fontIndex));
    Text::TextStyle style = Text::MakeTextStyle(textBox, layout.MaterialAt(textBox.materialIndex).description);

    Layout::PaneRectangle rectangle = Layout::RectangleOfPane(pane);
    Text::TextArea area = {rectangle.left, rectangle.top, rectangle.width, rectangle.height};
    Text::LaidOutText laidOutText = font.layouter.LayOut(textBox.text, style, area);
    return {laidOutText.block.width, laidOutText.block.height};
}

void MenuLayout::CopyTexture(std::string_view toPictureName, std::string_view fromPictureName)
{
    constexpr std::size_t FirstTextureMap = 0;

    Layout::LiveMaterial& source = MaterialOfPicture(fromPictureName);
    Layout::LiveMaterial& target = MaterialOfPicture(toPictureName);
    if (source.textureNames.empty() || target.textureNames.empty())
        throw MenuError(std::format("The pictures {} and {} of the layout {} need a texture each", fromPictureName, toPictureName, layoutName));

    target.textureNames[FirstTextureMap] = source.textureNames[FirstTextureMap];
}

void MenuLayout::MoveTo(Formats::Vector2 position)
{
    layout.RootPane().translation = {position.x, position.y, 0};
}

Formats::Vector2 MenuLayout::PlaceOf(std::string_view paneName)
{
    const Formats::Pane* wantedPane = &PaneNamed(paneName);
    for (const Layout::PanePlacement& placement : Layout::PlaceShownPanes(layout.RootPane(), placementSettings))
    {
        if (placement.pane == wantedPane)
            return Layout::TransformPoint(placement.matrix, {0, 0});
    }
    throw MenuError(std::format("The pane {} of the layout {} is not shown, so it has no place", paneName, layoutName));
}

std::vector<const Formats::Pane*> MenuLayout::PanesAt(Formats::Vector2 place)
{
    std::vector<Layout::PanePlacement> placements = Layout::PlaceShownPanes(layout.RootPane(), placementSettings);

    std::vector<const Formats::Pane*> panesAtPlace;
    for (auto placement = placements.rbegin(); placement != placements.rend(); ++placement)
    {
        std::optional<Formats::Vector2> placeInPane = Layout::FindPointMovedTo(placement->matrix, place);
        if (placeInPane && Layout::Contains(Layout::RectangleOfPane(*placement->pane), *placeInPane))
            panesAtPlace.push_back(placement->pane);
    }
    return panesAtPlace;
}

void MenuLayout::Calculate()
{
    FitToScreenShape();

    for (const std::unique_ptr<MenuAnimation>& animation : animations)
        animation->Step();
}

void MenuLayout::Draw()
{
    Layout::DrawLayout(display.Device(), layout, placementSettings, display.TextFonts());
}

void MenuLayout::Draw(const std::function<bool(const Formats::Pane&)>& isDrawn)
{
    for (const Layout::PanePlacement& placement : Layout::PlaceShownPanes(layout.RootPane(), placementSettings))
    {
        if (isDrawn(*placement.pane))
            Layout::DrawPlacedPane(display.Device(), layout, placement, display.TextFonts());
    }
}

// On a wide screen the root pane stretches the whole layout to fill the picture, and the panes that adjust their location are
// squeezed by the opposite so that they keep their shape. On a 4:3 screen nothing is stretched (docs/audit/OpenPoints/SystemMenu.md SYS-17).
void MenuLayout::FitToScreenShape()
{
    Formats::Vector2 stretch = display.LayoutStretch();
    layout.RootPane().scale = stretch;
    placementSettings.locationAdjustScale = {1 / stretch.x, 1 / stretch.y};
}

// The material that a picture draws with. Pictures of a layout use the materials of the layout by number.
Layout::LiveMaterial& MenuLayout::MaterialOfPicture(std::string_view pictureName)
{
    Formats::Pane& picture = PaneNamed(pictureName);
    if (!picture.picture)
        throw MenuError(std::format("The pane {} of the layout {} is not a picture", pictureName, layoutName));

    return layout.MaterialAt(picture.picture->materialIndex);
}

}
