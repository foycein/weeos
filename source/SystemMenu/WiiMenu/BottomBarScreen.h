#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <optional>
#include <vector>

namespace SystemMenu
{

// The buttons of the bar that the Wii Menu answers to.
enum class BarButton
{
    // The Wii button on the left: Wii Options.
    WiiButton,

    // The envelope on the right: the Message Board.
    MessageBoardButton,

    PreviousPage,
    NextPage,
};

// The bar at the bottom of the Wii Menu (screen 0x05, docs/research/SystemMenuStructure.md 4.3): the Wii button, the Message Board
// button and the arrows that turn the pages. The bar only draws; the channel grid asks it which button lies under a pointer and
// answers the presses itself, as in the original.
class BottomBarScreen : public Screen
{
public:
    explicit BottomBarScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The arrows show only where there is a page to go to.
    void ShowPageArrows(bool hasPreviousPage, bool hasNextPage);
    bool IsPreviousPageArrowShown();
    bool IsNextPageArrowShown();

    // The button under a place of the picture, in layout units. An arrow that is hidden is no button.
    std::optional<BarButton> ButtonAt(Formats::Vector2 place);

protected:
    bool RunMain() override;

private:
    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::vector<std::unique_ptr<FramePlayer>> stillGroups;
    std::unique_ptr<FramePlayer> arrowLoop;
};

}
