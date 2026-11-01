#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiMenu/Balloon.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"
#include "SystemMenu/WiiMenu/SdCardButton.h"

#include "Layout/LayoutArchive.h"

#include <array>
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

    // The SD card at the left: the SD Card Menu.
    SdCardButton,
};

// The bar at the bottom of the Wii Menu (screen 0x05, docs/research/SystemMenuStructure.md 4.3): the Wii button, the Message Board
// button and the arrows that turn the pages. The bar answers to the hands itself: a button that a hand comes onto grows and glows,
// plays WIPL_SE_BT_TARGETTING and, after a moment, shows a balloon with its name. The channel grid asks the bar which button lies
// under a pointer and answers the presses, as in the original.
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

    // A pressed arrow flashes.
    void FlashPageArrow(BarButton arrow);

    // The SD card button plays its press.
    void PressSdCardButton();

    // While a channel starts or the menu leaves, the buttons do not answer to the hands.
    void SetButtonsEnabled(bool shouldBeEnabled);

    bool IsFocused(BarButton button) const;
    const Balloon* BalloonOf(BarButton button) const;

    // The bar overlay draws the arrows and the balloons over the channel grid.
    void DrawPageArrows();
    void DrawBalloons();

    // The button under a place of the picture, in layout units. An arrow that is hidden is no button.
    std::optional<BarButton> ButtonAt(Formats::Vector2 place);

    // The middle of the area of a button in the picture, in layout units.
    Formats::Vector2 PlaceOfButton(BarButton button);

protected:
    bool RunMain() override;

private:
    // A button that answers to the hands: its animation of focus, and its balloon.
    struct FocusState
    {
        std::unique_ptr<FramePlayer> focus;
        std::unique_ptr<Balloon> balloon;
        bool isFocused = false;
    };

    void MakeTheFocusStates();
    void FocusTheButtonsUnderThePointers();
    void SetFocused(std::size_t buttonNumber, bool shouldBeFocused);
    void PlaceTheBalloons();
    void FollowTheSdCard();
    bool IsCardInTheSlot() const;
    void DrawArrow(std::string_view arrowName);
    void SlideArrow(FramePlayer& arrow, bool& isShown, bool shouldBeShown);
    bool IsShown(BarButton button) const;

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<Layout::LayoutArchive> balloonArchive;
    std::unique_ptr<MenuLayout> layout;
    std::vector<std::unique_ptr<FramePlayer>> stillGroups;
    std::vector<FocusState> focusStates;
    std::unique_ptr<FramePlayer> arrowLoop;
    std::unique_ptr<FramePlayer> previousPageArrow;
    std::unique_ptr<FramePlayer> nextPageArrow;
    std::unique_ptr<FramePlayer> previousPageFlash;
    std::unique_ptr<FramePlayer> nextPageFlash;
    std::unique_ptr<SdCardButton> sdCardButton;
    bool wasCardInTheSlot = false;
    bool isPreviousPageArrowShown = false;
    bool isNextPageArrowShown = false;
    bool areButtonsEnabled = true;
};

// Draws the page arrows and the balloons of the bar over the channel grid (screen 0x06). It opens last, so it is drawn after the grid.
class BarOverlayScreen : public Screen
{
public:
    explicit BarOverlayScreen(MenuServices& menuServices);

    void Draw(DrawPass pass) override;

private:
    MenuServices& services;
};

}
