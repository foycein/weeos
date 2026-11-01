#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <optional>
#include <string_view>
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

    // The arrows show only where there is a page to go to. An arrow that is not needed slides out of the picture.
    void ShowPageArrows(bool hasPreviousPage, bool hasNextPage);
    bool IsPreviousPageArrowShown() const;
    bool IsNextPageArrowShown() const;

    // The bar overlay draws the arrows over the channel grid.
    void DrawPageArrows();

    // The button under a place of the picture, in layout units. An arrow that is hidden is no button.
    std::optional<BarButton> ButtonAt(Formats::Vector2 place);

    // The middle of the area of a button in the picture, in layout units.
    Formats::Vector2 PlaceOfButton(BarButton button);

protected:
    bool RunMain() override;

private:
    void DrawArrow(std::string_view arrowName);
    void SlideArrow(FramePlayer& arrow, bool& isShown, bool shouldBeShown);

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::vector<std::unique_ptr<FramePlayer>> stillGroups;
    std::unique_ptr<FramePlayer> arrowLoop;
    std::unique_ptr<FramePlayer> previousPageArrow;
    std::unique_ptr<FramePlayer> nextPageArrow;
    bool isPreviousPageArrowShown = false;
    bool isNextPageArrowShown = false;
};

// Draws the page arrows of the bar over the channel grid (screen 0x06). It opens last, so it is drawn after the grid.
class BarOverlayScreen : public Screen
{
public:
    explicit BarOverlayScreen(MenuServices& menuServices);

    void Draw(DrawPass pass) override;

private:
    MenuServices& services;
};

}
