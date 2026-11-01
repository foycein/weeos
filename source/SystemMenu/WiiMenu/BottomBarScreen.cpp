#include "SystemMenu/WiiMenu/BottomBarScreen.h"

#include "SystemMenu/MenuFiles.h"

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

// The arrows bob up and down in frames 10000 to 10055 of the file, again and again.
constexpr std::string_view ArrowLoopGroup = "G_ArwRoop";
constexpr float FirstFrameOfTheArrowLoop = 10000;
constexpr float LastFrameOfTheArrowLoop = 10055;

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

    ShowPageArrows(false, false);
    layout->Calculate();
}

void BottomBarScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !layout)
        return;

    layout->Draw();
}

void BottomBarScreen::ShowPageArrows(bool hasPreviousPage, bool hasNextPage)
{
    layout->PaneNamed(PreviousPageArrow).isVisible = hasPreviousPage;
    layout->PaneNamed(NextPageArrow).isVisible = hasNextPage;
}

bool BottomBarScreen::IsPreviousPageArrowShown()
{
    return layout->PaneNamed(PreviousPageArrow).isVisible;
}

bool BottomBarScreen::IsNextPageArrowShown()
{
    return layout->PaneNamed(NextPageArrow).isVisible;
}

std::optional<BarButton> BottomBarScreen::ButtonAt(Formats::Vector2 place)
{
    for (const Formats::Pane* pane : layout->BoundingPanesAt(place))
    {
        for (const ButtonPane& buttonPane : ButtonPanes)
        {
            if (pane->name != buttonPane.paneName)
                continue;

            if (buttonPane.button == BarButton::PreviousPage && !IsPreviousPageArrowShown())
                continue;

            if (buttonPane.button == BarButton::NextPage && !IsNextPageArrowShown())
                continue;

            return buttonPane.button;
        }
    }
    return std::nullopt;
}

bool BottomBarScreen::RunMain()
{
    arrowLoop->Step();
    layout->Calculate();
    return false;
}

}
