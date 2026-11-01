#include "SystemMenu/WiiMenu/BottomBarScreen.h"

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Layout/LiveLayout.h"

#include <algorithm>
#include <array>
#include <string>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "cmnBtn";
constexpr std::string_view LayoutName = "my_IplTop_e";
constexpr std::string_view AnimationName = "my_IplTop_e";

// One animation file holds every state of the bar, each in its own frames. The groups below show frame 0, the look of the Wii Menu.
constexpr std::array<std::string_view, 12> GroupsOfTheBar = {
    "G_SeenChange", "G_Bbs", "G_Ch", "G_Set", "G_Cal", "G_Add", "G_CalExit", "G_AddExit", "G_Cmn_R", "G_Dust", "G_ArwR_Focus", "G_ArwL_Focus"};
constexpr float FrameOfTheWiiMenu = 0;

// The arrows bob sideways in frames 10000 to 10055 of the file, again and again.
constexpr std::string_view ArrowLoopGroup = "G_ArwRoop";
constexpr float FirstFrameOfTheArrowLoop = 10000;
constexpr float LastFrameOfTheArrowLoop = 10055;

// An arrow without a page behind it slides out of the picture in frames 10100 to 10110 and comes back in frames 10150 to 10160.
constexpr std::string_view PreviousPageArrowGroup = "G_ArwL_End";
constexpr std::string_view NextPageArrowGroup = "G_ArwR_End";
constexpr float FirstFrameOfSlidingOut = 10100;
constexpr float LastFrameOfSlidingOut = 10110;
constexpr float FirstFrameOfSlidingIn = 10150;
constexpr float LastFrameOfSlidingIn = 10160;

// The bar never draws its arrows: they would lie under the channel grid. The bar overlay draws them over it.
constexpr std::string_view PreviousPageArrow = "ArwL";
constexpr std::string_view NextPageArrow = "ArwR";

struct ButtonPane
{
    std::string_view paneName;
    BarButton button;
};

constexpr std::array<ButtonPane, 4> ButtonPanes = {{
    {"B_Set", BarButton::WiiButton},
    {"B_Bbs", BarButton::MessageBoardButton},
    {"B_ArwL", BarButton::PreviousPage},
    {"B_ArwR", BarButton::NextPage},
}};

}

BottomBarScreen::BottomBarScreen(MenuServices& menuServices)
    : Screen(ScreenId::BottomBar)
    , services(menuServices)
{
}

void BottomBarScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void BottomBarScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));

    for (std::string_view group : GroupsOfTheBar)
    {
        stillGroups.push_back(std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(group)));
        stillGroups.back()->Show(FrameOfTheWiiMenu);
    }

    arrowLoop = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(ArrowLoopGroup));
    arrowLoop->Loop(FirstFrameOfTheArrowLoop, LastFrameOfTheArrowLoop);

    previousPageArrow = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(PreviousPageArrowGroup));
    nextPageArrow = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(NextPageArrowGroup));
    previousPageArrow->Show(LastFrameOfSlidingOut);
    nextPageArrow->Show(LastFrameOfSlidingOut);

    layout->PaneNamed(PreviousPageArrow).isVisible = false;
    layout->PaneNamed(NextPageArrow).isVisible = false;
    layout->Calculate();
}

// The bar zooms with the grid: it is seen through the camera of the display (0x8139C49C). Its arrows are not (0x8139DA5C).
void BottomBarScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !layout)
        return;

    services.display.LookThroughCamera();
    layout->Draw();
    services.display.StopMovingAreas();
}

void BottomBarScreen::DrawPageArrows()
{
    DrawArrow(PreviousPageArrow);
    DrawArrow(NextPageArrow);
}

void BottomBarScreen::DrawArrow(std::string_view arrowName)
{
    Formats::Pane& arrow = layout->PaneNamed(arrowName);
    std::vector<Formats::Pane*> panesOfTheArrow = Layout::PaneWithDescendants(arrow);
    auto isPartOfTheArrow = [&](const Formats::Pane& pane) {
        return std::find(panesOfTheArrow.begin(), panesOfTheArrow.end(), &pane) != panesOfTheArrow.end();
    };

    arrow.isVisible = true;
    DrawPanes(*layout, services.display, PlacedPanes(*layout), isPartOfTheArrow);
    arrow.isVisible = false;
}

void BottomBarScreen::ShowPageArrows(bool hasPreviousPage, bool hasNextPage)
{
    SlideArrow(*previousPageArrow, isPreviousPageArrowShown, hasPreviousPage);
    SlideArrow(*nextPageArrow, isNextPageArrowShown, hasNextPage);
}

void BottomBarScreen::SlideArrow(FramePlayer& arrow, bool& isShown, bool shouldBeShown)
{
    if (isShown == shouldBeShown)
        return;

    isShown = shouldBeShown;
    if (isShown)
        arrow.PlayOnce(FirstFrameOfSlidingIn, LastFrameOfSlidingIn);
    else
        arrow.PlayOnce(FirstFrameOfSlidingOut, LastFrameOfSlidingOut);
}

bool BottomBarScreen::IsPreviousPageArrowShown() const
{
    return isPreviousPageArrowShown;
}

bool BottomBarScreen::IsNextPageArrowShown() const
{
    return isNextPageArrowShown;
}

std::optional<BarButton> BottomBarScreen::ButtonAt(Formats::Vector2 place)
{
    for (const Formats::Pane* pane : layout->BoundingPanesAt(place))
    {
        for (const ButtonPane& buttonPane : ButtonPanes)
        {
            if (pane->name != buttonPane.paneName)
                continue;

            if (buttonPane.button == BarButton::PreviousPage && !isPreviousPageArrowShown)
                continue;

            if (buttonPane.button == BarButton::NextPage && !isNextPageArrowShown)
                continue;

            return buttonPane.button;
        }
    }
    return std::nullopt;
}

Formats::Vector2 BottomBarScreen::PlaceOfButton(BarButton button)
{
    PlacedPanes placedPanes(*layout);
    for (const ButtonPane& buttonPane : ButtonPanes)
    {
        if (buttonPane.button == button)
            return placedPanes.PlaceOf(buttonPane.paneName).value_or(Formats::Vector2{0, 0});
    }
    return {0, 0};
}

bool BottomBarScreen::RunMain()
{
    arrowLoop->Step();
    previousPageArrow->Step();
    nextPageArrow->Step();
    layout->Calculate();
    return false;
}

BarOverlayScreen::BarOverlayScreen(MenuServices& menuServices)
    : Screen(ScreenId::BarOverlay)
    , services(menuServices)
{
}

void BarOverlayScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main)
        return;

    auto* bar = dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
    if (bar != nullptr)
        bar->DrawPageArrows();
}

}
