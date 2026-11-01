#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/ScreenId.h"
#include "SystemMenu/MessageBoard/LetterView.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/MessageBoard/SmallLetter.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace SystemMenu
{

class BottomBarScreen;

enum class BoardState
{
    // The channels are shown; the board waits.
    Hidden,

    // The bar takes the look of the board, then the letters of the day come.
    Arriving,
    Idle,
    TurningToNextDay,
    TurningToPreviousDay,
    HoldingLetter,
    ReadingLetter,

    // The calendar or the memo writer is open over the board.
    WaitingForScreen,
    LeavingForTheChannels,
};

// The Message Board: the main frame (screen 0x04) when the channel grid has left (docs/research/SystemMenuStructure.md 4.6; the states
// of 0x8138F168). It shows the letters of one day on the pictures of my_IplTop_c, turns the days with the arrows of the bar and with
// Plus and Minus, opens a letter, lets a letter be dragged to another place, and opens the calendar (0x07) and the memo writer (0x0A).
// The way back to the channels is the button on the right of the bar.
class BoardView
{
public:
    // The services, the data, the archive and the background with its animation belong to the main frame and outlive the board.
    BoardView(MenuServices& menuServices, MessageBoardData& boardData, const Layout::LayoutArchive& boardArchive, MenuLayout& background,
        FramePlayer& backgroundSlide);

    // The grid has gone: the board comes, with the letters of today.
    void Open();

    BoardState State() const;
    bool IsShown() const;

    void Run();

    // The letters, the mask over them and the open letter. The background is the main frame's.
    void Draw();

    // For the checks of the tests.
    const std::vector<std::unique_ptr<SmallLetter>>& Letters() const;
    LetterView* OpenLetter();
    Formats::Vector2 PlaceOfLetter(std::size_t letterIndex);
    int PageOfTheDay() const;

private:
    void LoadTheLettersOfTheDay();
    void ShowTheDay();
    void ShowTheArrows();
    void PlaceTheLetters();
    void RunIdle();
    void FocusTheLettersUnderThePointers();
    bool HandleDayButtons();
    bool HandleBarPress(int playerIndex, Formats::Vector2 place);
    bool HandleLetterPress(int playerIndex, Formats::Vector2 place);
    void TurnDay(int direction);
    void RunTurning();
    void RunHolding();
    void OpenTheLetter(std::size_t letterIndex);
    void RunReading();
    void OpenScreenOverTheBoard(ScreenId screen);
    void RunWaitingForScreen();
    void LeaveForTheChannels();
    void RunLeaving();
    std::optional<std::size_t> LetterUnder(Formats::Vector2 place);
    BottomBarScreen* Bar();
    Formats::Vector2 PlaceOfTheDay();

    MenuServices& services;
    MessageBoardData& data;
    const Layout::LayoutArchive& archive;
    MenuLayout& background;
    FramePlayer& backgroundSlide;
    std::unique_ptr<MenuLayout> mask;
    std::size_t maskIn = 0;
    std::size_t maskOut = 0;

    BoardState state = BoardState::Hidden;
    std::vector<StoredMessage> messagesOfTheDay;
    int page = 0;
    std::vector<std::unique_ptr<SmallLetter>> letters;
    std::array<std::optional<std::size_t>, PlayerCount> focusedLetters;
    std::unique_ptr<LetterView> openLetter;
    std::optional<ScreenId> screenOverTheBoard;

    int holdingPlayer = 0;
    std::size_t heldLetter = 0;
    Formats::Vector2 grabOffset{0, 0};
    Formats::Vector2 grabStart{0, 0};
    bool isDragging = false;
};

}
