#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/MessageBoard/BoardView.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"
#include "SystemMenu/WiiMenu/MenuConsole.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <optional>

namespace SystemMenu
{

// The Wii Menu (screen 0x04). It is the frame of the menu: it draws the background with the
// date below the clock and opens the bottom bar (0x05), the channel grid (0x02) and the bar overlay (0x06) as its children.
// When all three are there, the picture fades in. The same screen is the Message Board: when the envelope of the bar sends the
// channel grid away, the board (BoardView) shows its letters on the same background.
class MainFrameScreen : public Screen
{
public:
    explicit MainFrameScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The Message Board, while it is shown. Nothing in the Wii Menu, or when the menu runs without the board.
    BoardView* Board();

    // The frame is the background of its children, so it runs and draws while they are there.
    bool UpdatesWithChildren() const override;
    bool DrawsWithChildren() const override;

protected:
    void OnStart() override;
    bool RunMain() override;

private:
    void WriteTheDates(const LocalTime& today);
    bool AreTheChildrenBuilt();
    void ShowTheMessagesOfToday();
    void OpenTheBoardWhenTheGridHasGone();
    void AskForRoomWhenTheBoardIsNearlyFull();
    void ReadTheDayAgainAfterMakingRoom();

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> background;
    std::unique_ptr<FramePlayer> backgroundAnimation;
    std::unique_ptr<BoardView> board;
    std::optional<int> dayOfTheDates;
    bool hasFadedIn = false;
    bool hasShownTheMessageCount = false;
};

}
