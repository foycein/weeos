#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Layout/LayoutArchive.h"

#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The buttons of the bar. Which of them the bar has depends on what it shows (BarMode).
enum class BarButton
{
    // The Wii button on the left: Wii Options.
    WiiButton,

    // The envelope on the right: the Message Board.
    MessageBoardButton,

    PreviousPage,
    NextPage,

    // The buttons of the Message Board: the calendar and "Create Message" on the left, the way back to the channels on the right.
    CalendarButton,
    CreateMessageButton,
    ChannelsButton,

    // The buttons of a letter, the calendar and the memo writer: Back on the left, Trash, and the button on the right (Post).
    BackButton,
    TrashButton,
    RightButton,
};

// What the bar is showing, and so which buttons it has (the screen that listens to the bar in the original, 0x8139C93C).
enum class BarMode
{
    WiiMenu,
    MessageBoard,
    Letter,
    LetterWithRightButton,

    // A play log has only Back (the bar shows the frames 3113 to 3126 of 0x8139AAA0).
    PlayLog,
    Calendar,
    MemoWriter,
};

// The looks of the bar: parts of its animation file that move its buttons from one state into another
// (the table 0x8160F7B8 that 0x8139CB58 plays; the numbers are the original's).
enum class BarChange
{
    ToTheBoard,               // 0: frames 1000 to 1040
    ToTheChannels,            // 3: 6000 to 6040
    CalendarOpens,            // 4: 2000 to 2050
    CalendarClosesWithBack,   // 5: 3000 to 3050
    CalendarClosesWithADay,   // 6: 3500 to 3550
    CreateMessageOpens,       // 7: 4000 to 4026
    LetterOpens,              // 9: 3100 to 3113
    BackToTheBoard,           // 10: 3426 to 3439
    PlayLogShows,             // 0x0B: 3113 to 3126
    LetterShowsWithoutButton, // 0x0D: 3600 to 3613
    MemoWriterShows,          // 0x0F: 3313 to 3326
    MemoWriterCloses,         // 0x10: 3413 to 3426
    LetterShowsWithButton,    // 0x11: 3640 to 3653
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

    // Which buttons the bar has.
    void SetMode(BarMode newMode);
    BarMode Mode() const;

    // The bar moves into another look. A change that is asked for while another one plays waits for it, in order.
    void Change(BarChange change);
    bool IsChanging() const;

    // The words of the button on the left (Back, Quit), of the button on the right (Post) and of the trash.
    void SetBackText(std::u16string_view text);
    void SetRightText(std::u16string_view text);
    void SetTrashText(std::u16string_view text);

    // The buttons of the bar flash when they are pressed (G_CalExit, G_Dust and G_Cmn_R, frames 3000 to 3020 and 2800 to 2820).
    void FlashButton(BarButton button);

    // The number of the day's messages on the envelope, 0 to 99. Without messages the mark is hidden (0x8138F168, 0x8139D004).
    void ShowMessageCount(int count);

protected:
    bool RunMain() override;

private:
    void DrawArrow(std::string_view arrowName);
    void SlideArrow(FramePlayer& arrow, bool& isShown, bool shouldBeShown);
    bool HasButtonInThisMode(BarButton button) const;
    void StartTheNextChange();
    std::unique_ptr<FramePlayer> MakeGroupPlayer(std::string_view groupName);

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::vector<std::unique_ptr<FramePlayer>> stillGroups;
    std::unique_ptr<FramePlayer> arrowLoop;
    std::unique_ptr<FramePlayer> previousPageArrow;
    std::unique_ptr<FramePlayer> nextPageArrow;
    std::unique_ptr<FramePlayer> seenChange;
    std::unique_ptr<FramePlayer> backFlash;
    std::unique_ptr<FramePlayer> trashFlash;
    std::unique_ptr<FramePlayer> rightFlash;
    std::unique_ptr<FramePlayer> messageSignal;
    std::deque<BarChange> waitingChanges;
    BarMode mode = BarMode::WiiMenu;
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
