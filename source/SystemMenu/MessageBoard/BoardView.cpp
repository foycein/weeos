#include "SystemMenu/MessageBoard/BoardView.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/MessageBoard/BoardInput.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/ClockFace.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Text/MessageCatalog.h"

#include <algorithm>
#include <cmath>

namespace SystemMenu
{

namespace
{

constexpr std::string_view MaskLayout = "my_BbsMask_a";

// The three pictures of the background hold the day before, the day of the board and the day after (0x81391F84).
constexpr std::array<std::string_view, 3> DatePanes = {"T_Day_a", "T_Day_b", "T_Day_c"};
constexpr std::string_view PictureOfTheDay = "TopBack_b";

// The background slides to the next day in frames 30 to 50 of its animation and to the day before in frames 0 to 20 (0x813930DC).
constexpr float FrameAtRest = 0;
constexpr float FirstFrameToNextDay = 30;
constexpr float LastFrameToNextDay = 50;
constexpr float FirstFrameToPreviousDay = 0;
constexpr float LastFrameToPreviousDay = 20;

// A day shows ten letters at a time (0x81393C10).
constexpr std::size_t LettersPerPage = 10;

// A letter that the hand moves further than this, with A held, is dragged instead of opened.
constexpr float DistanceThatDrags = 4;

// The texts of the bar over the calendar and the memo writer (0x813935D4, 0x813C30C4).
constexpr std::uint16_t BackMessage = 35;
constexpr std::uint16_t MemoBackMessage = 79;
constexpr std::uint16_t PostMessage = 36;

constexpr std::string_view FocusSound = "WIPL_SE_BOARD_FOCUS";
constexpr std::string_view HoldSound = "WIPL_SE_BOARD_HOLD";
constexpr std::string_view DragSound = "WIPL_SE_BOARD_DRAG";
constexpr std::string_view ReleaseSound = "WIPL_SE_BOARD_RELEASE";
constexpr std::string_view SelectSound = "WIPL_SE_BOARD_SELECT";
constexpr std::string_view LetterAppearsSound = "WIPL_SE_MSG_DISP";
constexpr std::string_view NextPageSound = "WIPL_SE_MSG_HOUSE";
constexpr std::string_view DaySound = "WSD_SELECT";

// The board comes back to the channels with the grid in front of the bar overlay, as the original inserts it (0x8136276C).
constexpr int GridComesFromTheBoard = 1;

// The date of a picture is written for the middle of its day.
constexpr int NoonHour = 12;
constexpr int OffsetOfTheFirstPicture = -1;

LocalTime NoonOf(BoardDay day)
{
    return LocalTime{day.year, day.month, day.day, NoonHour, 0, 0};
}

std::size_t PageCountOf(std::size_t messageCount)
{
    return (messageCount + LettersPerPage - 1) / LettersPerPage;
}

}

BoardView::BoardView(MenuServices& menuServices, MessageBoardData& boardData, const Layout::LayoutArchive& boardArchive,
    MenuLayout& backgroundLayout, FramePlayer& backgroundSlidePlayer)
    : services(menuServices)
    , data(boardData)
    , archive(boardArchive)
    , background(backgroundLayout)
    , backgroundSlide(backgroundSlidePlayer)
{
    mask = std::make_unique<MenuLayout>(services.display, archive, std::string(MaskLayout));
    maskIn = mask->BindAnimationToAllPanes(std::string(MaskLayout) + "_MaskIn");
    maskOut = mask->BindAnimationToAllPanes(std::string(MaskLayout) + "_MaskOut");
    mask->Calculate();
}

void BoardView::Open()
{
    data.shownDay = DayOf(services.wiiMenu.console.GetLocalTime());
    if (BottomBarScreen* bar = Bar())
        bar->SetMode(BarMode::MessageBoard);
    ShowTheDay();
    state = BoardState::Arriving;
}

BoardState BoardView::State() const
{
    return state;
}

bool BoardView::IsShown() const
{
    return state != BoardState::Hidden;
}

const std::vector<std::unique_ptr<SmallLetter>>& BoardView::Letters() const
{
    return letters;
}

LetterView* BoardView::OpenLetter()
{
    return openLetter.get();
}

Formats::Vector2 BoardView::PlaceOfLetter(std::size_t letterIndex)
{
    Formats::Vector2 dayPlace = PlaceOfTheDay();
    Formats::Vector2 letterPlace = letters.at(letterIndex)->PlaceOnTheDay();
    return Formats::Vector2{dayPlace.x + letterPlace.x, dayPlace.y + letterPlace.y};
}

int BoardView::PageOfTheDay() const
{
    return page;
}

BottomBarScreen* BoardView::Bar()
{
    return dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
}

// The dates of the three pictures and the letters of the first page of the day.
void BoardView::ShowTheDay()
{
    int dayOffset = OffsetOfTheFirstPicture;
    for (std::string_view datePane : DatePanes)
    {
        background.SetText(datePane, MakeDateText(NoonOf(ShiftDays(data.shownDay, dayOffset)), services.messages));
        ++dayOffset;
    }

    backgroundSlide.Show(FrameAtRest);
    messagesOfTheDay = data.store.MessagesOfDay(data.shownDay);
    page = 0;
    LoadTheLettersOfTheDay();
    ShowTheArrows();
}

void BoardView::LoadTheLettersOfTheDay()
{
    letters.clear();
    focusedLetters = {};
    std::uint64_t ticksNow = TicksOf(services.wiiMenu.console.GetLocalTime());
    std::size_t first = static_cast<std::size_t>(page) * LettersPerPage;
    std::size_t end = std::min(messagesOfTheDay.size(), first + LettersPerPage);
    for (std::size_t index = first; index < end; ++index)
    {
        letters.push_back(std::make_unique<SmallLetter>(services.display, archive, messagesOfTheDay[index], data.portraits, ticksNow));
        letters.back()->Paste();
    }

    if (!letters.empty())
        services.sounds.Request(LetterAppearsSound);
    PlaceTheLetters();
}

// The arrows of the bar show while there is a page or a day to go to (0x8139294C).
void BoardView::ShowTheArrows()
{
    BottomBarScreen* bar = Bar();
    if (bar == nullptr)
        return;

    bool hasMorePages = (static_cast<std::size_t>(page) + 1) * LettersPerPage < messagesOfTheDay.size();
    bool hasNext = hasMorePages || data.shownDay < LastBoardDay;
    bool hasPrevious = page > 0 || data.shownDay > FirstBoardDay;
    bar->ShowPageArrows(hasPrevious, hasNext);
}

Formats::Vector2 BoardView::PlaceOfTheDay()
{
    PlacedPanes placedPanes(background);
    return placedPanes.PlaceOf(PictureOfTheDay).value_or(Formats::Vector2{0, 0});
}

void BoardView::PlaceTheLetters()
{
    Formats::Vector2 dayPlace = PlaceOfTheDay();
    for (std::unique_ptr<SmallLetter>& letter : letters)
    {
        letter->PutDayAt(dayPlace);
        letter->Calculate();
    }
}

void BoardView::Run()
{
    switch (state)
    {
    case BoardState::Hidden:
        return;

    case BoardState::Arriving:
        if (Bar() == nullptr || !Bar()->IsChanging())
            state = BoardState::Idle;
        break;

    case BoardState::Idle:
        RunIdle();
        break;

    case BoardState::TurningToNextDay:
    case BoardState::TurningToPreviousDay:
        RunTurning();
        break;

    case BoardState::HoldingLetter:
        RunHolding();
        break;

    case BoardState::ReadingLetter:
        RunReading();
        break;

    case BoardState::WaitingForScreen:
        RunWaitingForScreen();
        break;

    case BoardState::LeavingForTheChannels:
        RunLeaving();
        break;
    }

    mask->Calculate();
    PlaceTheLetters();
}

void BoardView::RunIdle()
{
    FocusTheLettersUnderThePointers();
    if (services.fader.GetStatus() != FaderStatus::Clear)
        return;
    if (HandleDayButtons())
        return;

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        if (!player || !place || !HasGoneDown(*player, BoardPressButtons))
            continue;

        if (HandleBarPress(*place) || HandleLetterPress(playerIndex, *place))
            return;
    }
}

// The letter in front, the one drawn last, is the one under the pointer.
std::optional<std::size_t> BoardView::LetterUnder(Formats::Vector2 place)
{
    for (std::size_t index = letters.size(); index > 0; --index)
    {
        if (letters[index - 1]->IsPointedAt(place))
            return index - 1;
    }
    return std::nullopt;
}

void BoardView::FocusTheLettersUnderThePointers()
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        std::optional<std::size_t> underThePointer = place ? LetterUnder(*place) : std::nullopt;
        std::optional<std::size_t>& focused = focusedLetters[playerIndex];
        if (underThePointer == focused)
            continue;

        if (focused)
            letters[*focused]->Unfocus();
        if (underThePointer)
        {
            letters[*underThePointer]->Focus();
            services.sounds.Request(FocusSound);
        }
        focused = underThePointer;
    }
}

bool BoardView::HandleDayButtons()
{
    if (services.input.AnyPlayerPressed(BoardNextButtons))
    {
        TurnDay(1);
        return true;
    }
    if (services.input.AnyPlayerPressed(BoardPreviousButtons))
    {
        TurnDay(-1);
        return true;
    }
    return false;
}

bool BoardView::HandleBarPress(Formats::Vector2 place)
{
    BottomBarScreen* bar = Bar();
    std::optional<BarButton> button = bar ? bar->ButtonAt(place) : std::nullopt;
    if (!button)
        return false;

    switch (*button)
    {
    case BarButton::NextPage:
        TurnDay(1);
        return true;

    case BarButton::PreviousPage:
        TurnDay(-1);
        return true;

    case BarButton::ChannelsButton:
        LeaveForTheChannels();
        return true;

    case BarButton::CalendarButton:
        services.sounds.Request(SoundName::Decide);
        bar->SetBackText(services.messages.Find(BackMessage));
        bar->SetMode(BarMode::Calendar);
        bar->Change(BarChange::CalendarOpens);
        OpenScreenOverTheBoard(ScreenId::Calendar);
        return true;

    case BarButton::CreateMessageButton:
        services.sounds.Request(SoundName::Decide);
        bar->ShowPageArrows(false, false);
        bar->SetBackText(services.messages.Find(MemoBackMessage));
        bar->SetRightText(services.messages.Find(PostMessage));
        bar->SetMode(BarMode::MemoWriter);
        bar->Change(BarChange::CreateMessageOpens);
        bar->Change(BarChange::MemoWriterShows);
        OpenScreenOverTheBoard(ScreenId::MemoWriter);
        return true;

    default:
        return false;
    }
}

bool BoardView::HandleLetterPress(int playerIndex, Formats::Vector2 place)
{
    std::optional<std::size_t> letterIndex = LetterUnder(place);
    if (!letterIndex)
        return false;

    // The held letter is drawn over the others (0x8138FE40).
    std::unique_ptr<SmallLetter> letter = std::move(letters[*letterIndex]);
    letters.erase(letters.begin() + static_cast<std::ptrdiff_t>(*letterIndex));
    letters.push_back(std::move(letter));
    focusedLetters = {};

    heldLetter = letters.size() - 1;
    holdingPlayer = playerIndex;
    Formats::Vector2 dayPlace = PlaceOfTheDay();
    Formats::Vector2 letterPlace = letters[heldLetter]->PlaceOnTheDay();
    grabOffset = Formats::Vector2{letterPlace.x - (place.x - dayPlace.x), letterPlace.y - (place.y - dayPlace.y)};
    grabStart = place;
    isDragging = false;
    services.sounds.Request(HoldSound);
    state = BoardState::HoldingLetter;
    return true;
}

// The held letter follows the hand. Let go where it was taken, it opens; let go elsewhere, it stays there (0x81394CD4).
void BoardView::RunHolding()
{
    const std::optional<PlayerInput>& player = services.input.Player(holdingPlayer);
    std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, holdingPlayer);
    SmallLetter& letter = *letters[heldLetter];
    if (place)
    {
        float distance = std::hypot(place->x - grabStart.x, place->y - grabStart.y);
        if (!isDragging && distance > DistanceThatDrags)
        {
            isDragging = true;
            services.sounds.Request(DragSound);
        }
        if (isDragging)
        {
            Formats::Vector2 dayPlace = PlaceOfTheDay();
            letter.MoveOnTheDay(Formats::Vector2{place->x - dayPlace.x + grabOffset.x, place->y - dayPlace.y + grabOffset.y});
        }
    }

    bool isStillHeld = player && IsHeldDown(*player, BoardPressButtons);
    if (isStillHeld)
        return;

    if (!isDragging)
    {
        OpenTheLetter(heldLetter);
        return;
    }

    services.sounds.Request(ReleaseSound);
    data.store.ChangeMessage(letter.Message(), services.wiiMenu.console.GetLocalTime());
    for (StoredMessage& stored : messagesOfTheDay)
    {
        if (stored.place == letter.Message().place)
            stored = letter.Message();
    }
    state = BoardState::Idle;
}

void BoardView::OpenTheLetter(std::size_t letterIndex)
{
    BottomBarScreen* bar = Bar();
    if (bar == nullptr)
        return;

    SmallLetter& letter = *letters[letterIndex];
    letter.Select();
    services.sounds.Request(SelectSound);
    mask->StartAnimation(maskIn);
    bar->ShowPageArrows(false, false);

    Formats::Vector2 dayPlace = PlaceOfTheDay();
    Formats::Vector2 letterPlace = letter.PlaceOnTheDay();
    Formats::Vector2 placeInPicture{dayPlace.x + letterPlace.x, dayPlace.y + letterPlace.y};
    openLetter = std::make_unique<LetterView>(services, archive, letter.Message(), data.portraits, *bar, placeInPicture);
    state = BoardState::ReadingLetter;
}

void BoardView::RunReading()
{
    LetterViewResult result = openLetter->Run();
    if (result == LetterViewResult::StillOpen)
        return;

    if (result == LetterViewResult::Thrown)
        data.store.DeleteMessage(letters[heldLetter]->Message().place);

    openLetter.reset();
    mask->StartAnimation(maskOut);
    if (BottomBarScreen* bar = Bar())
        bar->SetMode(BarMode::MessageBoard);

    std::size_t pageBefore = static_cast<std::size_t>(page);
    messagesOfTheDay = data.store.MessagesOfDay(data.shownDay);
    std::size_t pageCount = PageCountOf(messagesOfTheDay.size());
    std::size_t lastPage = pageCount == 0 ? 0 : pageCount - 1;
    page = static_cast<int>(std::min(pageBefore, lastPage));
    LoadTheLettersOfTheDay();
    ShowTheArrows();
    state = BoardState::Idle;
}

// The arrows turn the pages of a day with more than ten letters first, then the days (0x81393970, 0x81392F2C, 0x81392F94).
void BoardView::TurnDay(int direction)
{
    std::size_t pageCount = PageCountOf(messagesOfTheDay.size());
    bool hasNextPage = static_cast<std::size_t>(page) + 1 < pageCount;
    if (direction > 0 && hasNextPage)
    {
        ++page;
        services.sounds.Request(NextPageSound);
        LoadTheLettersOfTheDay();
        ShowTheArrows();
        return;
    }
    if (direction < 0 && page > 0)
    {
        --page;
        services.sounds.Request(NextPageSound);
        LoadTheLettersOfTheDay();
        ShowTheArrows();
        return;
    }

    if (direction > 0 && data.shownDay >= LastBoardDay)
        return;
    if (direction < 0 && data.shownDay <= FirstBoardDay)
        return;

    services.sounds.Request(DaySound);
    focusedLetters = {};
    if (direction > 0)
    {
        backgroundSlide.PlayOnce(FirstFrameToNextDay, LastFrameToNextDay);
        state = BoardState::TurningToNextDay;
    }
    else
    {
        backgroundSlide.PlayOnce(FirstFrameToPreviousDay, LastFrameToPreviousDay);
        state = BoardState::TurningToPreviousDay;
    }
}

// The letters ride on the picture of their day; when the next picture is in the middle, the board shows the new day there.
void BoardView::RunTurning()
{
    if (backgroundSlide.IsPlaying())
        return;

    int direction = state == BoardState::TurningToNextDay ? 1 : -1;
    data.shownDay = ShiftDays(data.shownDay, direction);
    ShowTheDay();
    state = BoardState::Idle;
}

// The bar stays over the calendar and the memo writer, so that their Back and Post can be seen.
void BoardView::OpenScreenOverTheBoard(ScreenId screen)
{
    mask->StartAnimation(maskIn);
    services.screens.OpenChildBefore(screen, ScreenId::MainFrame, ScreenId::BottomBar, 0);
    screenOverTheBoard = screen;
    state = BoardState::WaitingForScreen;
}

// When the calendar or the memo writer is gone, the board shows its day again: the calendar may have chosen another day, the memo
// writer may have put a memo on it.
void BoardView::RunWaitingForScreen()
{
    bool hasAppeared = services.screens.FindScreen(*screenOverTheBoard) != nullptr || services.screens.IsLoadingScreen();
    if (hasAppeared)
        return;

    mask->StartAnimation(maskOut);
    if (BottomBarScreen* bar = Bar())
        bar->SetMode(BarMode::MessageBoard);
    screenOverTheBoard.reset();
    ShowTheDay();
    state = BoardState::Idle;
}

void BoardView::LeaveForTheChannels()
{
    services.sounds.Request(SoundName::Decide);
    if (BottomBarScreen* bar = Bar())
    {
        bar->SetMode(BarMode::WiiMenu);
        bar->Change(BarChange::ToTheChannels);
    }
    services.screens.OpenChildBefore(ScreenId::ChannelGrid, ScreenId::MainFrame, ScreenId::BarOverlay, GridComesFromTheBoard);
    state = BoardState::LeavingForTheChannels;
}

// The letters go when the grid stands in front of them.
void BoardView::RunLeaving()
{
    Screen* grid = services.screens.FindScreen(ScreenId::ChannelGrid);
    if (grid == nullptr || grid->GetState() != ScreenState::Running)
        return;

    letters.clear();
    messagesOfTheDay.clear();
    data.shownDay = DayOf(services.wiiMenu.console.GetLocalTime());
    state = BoardState::Hidden;
}

void BoardView::Draw()
{
    if (state == BoardState::Hidden)
        return;

    for (std::unique_ptr<SmallLetter>& letter : letters)
        letter->Draw();

    mask->Draw();
    if (openLetter)
        openLetter->Draw();
}

}
