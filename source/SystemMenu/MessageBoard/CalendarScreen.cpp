#include "SystemMenu/MessageBoard/CalendarScreen.h"

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MessageBoard/BoardInput.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Layout/LiveLayout.h"
#include "Text/MessageCatalog.h"

#include <algorithm>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "calendar";
constexpr std::string_view MonthLayout = "my_IplTop_g";
constexpr std::string_view CellLayout = "my_IplTop_f";

// The month slides in frames of my_IplTop_g: the calendar opens in 1000 to 1040, turns to the next month in 2000 to 2030 and to the
// month before in 3000 to 3030, and closes in 4000 to 4020.
constexpr float FirstFrameOfOpening = 1000;
constexpr float LastFrameOfOpening = 1040;
constexpr float FirstFrameToNextMonth = 2000;
constexpr float LastFrameToNextMonth = 2030;
constexpr float FirstFrameToPreviousMonth = 3000;
constexpr float LastFrameToPreviousMonth = 3030;
constexpr float FirstFrameOfClosing = 4000;
constexpr float LastFrameOfClosing = 4020;

// The three month grids of the layout: the month that shows in the middle, the month before on the left, the month after on the right.
constexpr std::string_view ShownGrid = "b";
constexpr std::string_view PreviousGrid = "a";
constexpr std::string_view NextGrid = "c";

// The days stand 70 units apart sideways and 48 units apart downward from the place N_CalPos of their grid (0x8139F3B0).
constexpr float CellStepSideways = 70;
constexpr float CellStepDownward = 48;

// The names of the days over the columns, Sunday first, for each grid (the text boxes from left to right).
constexpr std::array<std::string_view, DaysPerWeek> WeekdayBoxesOfShownGrid = {
    "TextBox_08", "TextBox_09", "TextBox_10", "TextBox_11", "TextBox_12", "TextBox_13", "TextBox_07"};
constexpr std::array<std::string_view, DaysPerWeek> WeekdayBoxesOfPreviousGrid = {
    "TextBox_00", "TextBox_01", "TextBox_02", "TextBox_03", "TextBox_04", "TextBox_05", "TextBox_06"};
constexpr std::array<std::string_view, DaysPerWeek> WeekdayBoxesOfNextGrid = {
    "TextBox_16", "TextBox_17", "TextBox_18", "TextBox_19", "TextBox_20", "TextBox_14", "TextBox_15"};

// The messages of the names of the days, Sunday first, and of the months, January first (0x8160FA08, 0x8160FAB0).
constexpr std::array<std::uint16_t, DaysPerWeek> WeekdayMessages = {116, 110, 111, 112, 113, 114, 115};
constexpr std::uint16_t JanuaryMessage = 118;

// The parts of the animation of a day (the table 0x8160FBA0): which pane and which frames.
constexpr std::string_view FocusPane = "N_CalDay_r";
constexpr std::string_view TodayPane = "W_Cal";
constexpr std::string_view WeekdayPane = "T_Cal";
constexpr std::string_view SelectPane = "Cal_Ac";
constexpr std::string_view MarkPane = "Info_a";
constexpr std::string_view DayButton = "B_Cal";
constexpr std::string_view DayNumberText = "T_Cal";

constexpr float FirstFrameOfFocusIn = 0;
constexpr float LastFrameOfFocusIn = 6;
constexpr float FirstFrameOfFocusOut = 10;
constexpr float LastFrameOfFocusOut = 18;
constexpr float FirstFrameOfSelecting = 50;
constexpr float LastFrameOfSelecting = 80;
constexpr float FrameOfToday = 1;
constexpr float FrameOfAnotherDay = 0;
constexpr float FrameOfAnotherMonth = 3;
constexpr float FrameOfAWeekday = 0;
constexpr float FrameOfSaturday = 1;
constexpr float FrameOfSunday = 2;
constexpr float FrameWithMark = 1;
constexpr float FrameWithoutMark = 0;
constexpr int Sunday = 0;
constexpr int Saturday = 6;

constexpr std::string_view ScrollSound = "WIPL_SE_CALENDAR_SCROLL";
constexpr std::string_view SelectSound = "WIPL_SE_DATE_SELECT";
constexpr std::string_view FocusSound = "WIPL_SE_DATE_FOCUS";

std::u16string NumberText(int number)
{
    std::string digits = std::to_string(number);
    return std::u16string(digits.begin(), digits.end());
}

std::unique_ptr<FramePlayer> MakePartPlayer(MenuLayout& layout, const Layout::LayoutArchive& archive, std::string_view paneName)
{
    std::vector<Formats::Pane*> panes = {&layout.PaneNamed(paneName)};
    return std::make_unique<FramePlayer>(layout, archive, CellLayout, panes);
}

}

CalendarDayView::CalendarDayView(Display& display, const Layout::LayoutArchive& archive)
    : layout(std::make_unique<MenuLayout>(display, archive, std::string(CellLayout)))
{
    focusPart = MakePartPlayer(*layout, archive, FocusPane);
    todayPart = MakePartPlayer(*layout, archive, TodayPane);
    weekdayPart = MakePartPlayer(*layout, archive, WeekdayPane);
    selectPart = MakePartPlayer(*layout, archive, SelectPane);
    markPart = MakePartPlayer(*layout, archive, MarkPane);
    focusPart->Show(FirstFrameOfFocusIn);
    selectPart->Show(FirstFrameOfSelecting);
}

// A day of another month is gray and cannot be chosen (0x8139FD6C, 0x8139FD24).
void CalendarDayView::ShowDay(BoardDay shownDay, bool isOfTheMonth, bool isToday, bool hasLetters, const std::u16string& number)
{
    day = shownDay;
    isInTheMonth = isOfTheMonth;
    layout->SetText(DayNumberText, number);

    if (!isInTheMonth)
    {
        weekdayPart->Show(FrameOfAnotherMonth);
        todayPart->Show(FrameOfAnotherMonth);
    }
    else
    {
        int weekday = WeekdayOf(day);
        float weekdayFrame = weekday == Saturday ? FrameOfSaturday : (weekday == Sunday ? FrameOfSunday : FrameOfAWeekday);
        weekdayPart->Show(weekdayFrame);
        todayPart->Show(isToday ? FrameOfToday : FrameOfAnotherDay);
    }

    markPart->Show(isInTheMonth && hasLetters ? FrameWithMark : FrameWithoutMark);
    layout->PaneNamed(FocusPane).isVisible = isInTheMonth;
}

BoardDay CalendarDayView::Day() const
{
    return day;
}

bool CalendarDayView::IsInTheMonth() const
{
    return isInTheMonth;
}

bool CalendarDayView::IsPointedAt(Formats::Vector2 place)
{
    if (!isInTheMonth)
        return false;

    std::vector<const Formats::Pane*> buttons = layout->BoundingPanesAt(place);
    return std::any_of(buttons.begin(), buttons.end(), [](const Formats::Pane* pane) { return pane->name == DayButton; });
}

std::optional<Formats::Vector2> CalendarDayView::PlaceOfButton()
{
    return layout->PointOfPaneInPicture(DayButton, Formats::Vector2{0, 0});
}

void CalendarDayView::Focus()
{
    focusPart->PlayOnce(FirstFrameOfFocusIn, LastFrameOfFocusIn);
}

void CalendarDayView::Unfocus()
{
    focusPart->PlayOnce(FirstFrameOfFocusOut, LastFrameOfFocusOut);
}

void CalendarDayView::Select()
{
    selectPart->PlayOnce(FirstFrameOfSelecting, LastFrameOfSelecting);
}

bool CalendarDayView::IsSelecting() const
{
    return selectPart->IsPlaying();
}

void CalendarDayView::PutAt(Formats::Vector2 place)
{
    MoveLayoutTo(*layout, place);
}

void CalendarDayView::Calculate()
{
    for (FramePlayer* part : {focusPart.get(), todayPart.get(), weekdayPart.get(), selectPart.get(), markPart.get()})
        part->Step();
    layout->Calculate();
}

void CalendarDayView::Draw()
{
    layout->Draw();
}

CalendarScreen::CalendarScreen(MenuServices& menuServices)
    : Screen(ScreenId::Calendar)
    , services(menuServices)
    , data(*menuServices.messageBoard)
{
}

void CalendarScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void CalendarScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(MonthLayout));
    monthSlide = std::make_unique<FramePlayer>(*layout, *archive, MonthLayout);
    monthSlide->PlayOnce(FirstFrameOfOpening, LastFrameOfOpening);

    for (std::size_t index = 0; index < CalendarCellCount; ++index)
    {
        cells[index] = std::make_unique<CalendarDayView>(services.display, *archive);
        incomingCells[index] = std::make_unique<CalendarDayView>(services.display, *archive);
    }

    for (std::size_t index = 0; index < DaysPerWeek; ++index)
    {
        const std::u16string& dayName = services.messages.Find(WeekdayMessages[index]);
        layout->SetText(WeekdayBoxesOfShownGrid[index], dayName);
        layout->SetText(WeekdayBoxesOfPreviousGrid[index], dayName);
        layout->SetText(WeekdayBoxesOfNextGrid[index], dayName);
    }

    year = data.shownDay.year;
    month = data.shownDay.month;
    ShowTheMonth(year, month, cells, ShownGrid);
    ShowTheArrows();
    layout->Calculate();
}

bool CalendarScreen::IsReady()
{
    return true;
}

int CalendarScreen::ShownYear() const
{
    return year;
}

int CalendarScreen::ShownMonth() const
{
    return month;
}

const std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount>& CalendarScreen::Cells() const
{
    return cells;
}

BottomBarScreen* CalendarScreen::Bar()
{
    return dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
}

// The month title is the name of the month and the year, "November 2026" (0x8139EE30); the days with letters carry a mark (0x8139F5EC).
void CalendarScreen::ShowTheMonth(int shownYear, int shownMonth, std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount>& monthCells,
    std::string_view gridLetter)
{
    std::u16string title = services.messages.Find(static_cast<std::uint16_t>(JanuaryMessage + shownMonth - 1)) + u" " + NumberText(shownYear);
    layout->SetText(std::format("T_CalMonth_{}", gridLetter), title);

    BoardDay today = DayOf(services.wiiMenu.console.GetLocalTime());
    std::array<SystemMenu::CalendarCell, CalendarCellCount> page = MakeCalendarPage(shownYear, shownMonth);
    for (std::size_t index = 0; index < CalendarCellCount; ++index)
    {
        const SystemMenu::CalendarCell& cellOfThePage = page[index];
        bool hasLetters = cellOfThePage.isInTheMonth && data.store.HasLettersOnDay(cellOfThePage.day);
        monthCells[index]->ShowDay(cellOfThePage.day, cellOfThePage.isInTheMonth, cellOfThePage.day == today, hasLetters,
            NumberText(cellOfThePage.day.day));
    }
    PlaceCells(monthCells, gridLetter);
}

void CalendarScreen::PlaceCells(std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount>& monthCells, std::string_view gridLetter)
{
    PlacedPanes placedPanes(*layout);
    Formats::Vector2 corner = placedPanes.PlaceOf(std::format("N_CalPos_{}", gridLetter)).value_or(Formats::Vector2{0, 0});
    for (std::size_t index = 0; index < CalendarCellCount; ++index)
    {
        float column = static_cast<float>(index % DaysPerWeek);
        float row = static_cast<float>(index / DaysPerWeek);
        monthCells[index]->PutAt(Formats::Vector2{corner.x + column * CellStepSideways, corner.y - row * CellStepDownward});
        monthCells[index]->Calculate();
    }
}

void CalendarScreen::ShowTheArrows()
{
    BottomBarScreen* bar = Bar();
    if (bar == nullptr)
        return;

    bool hasPreviousMonth = year > FirstBoardDay.year || month > FirstBoardDay.month;
    bool hasNextMonth = year < LastBoardDay.year || month < LastBoardDay.month;
    bar->ShowPageArrows(hasPreviousMonth, hasNextMonth);
}

bool CalendarScreen::RunMain()
{
    bool hasEnded = false;
    switch (state)
    {
    case CalendarState::Opening:
        if (!monthSlide->IsPlaying())
            state = CalendarState::Choosing;
        break;

    case CalendarState::Choosing:
        RunChoosing();
        break;

    case CalendarState::TurningToNextMonth:
    case CalendarState::TurningToPreviousMonth:
        if (!monthSlide->IsPlaying())
        {
            monthSlide->Show(LastFrameOfOpening);
            hasIncomingMonth = false;
            ShowTheMonth(year, month, cells, ShownGrid);
            ShowTheArrows();
            state = CalendarState::Choosing;
        }
        break;

    case CalendarState::ShowingTheChoice:
        if (!cells[chosenCell]->IsSelecting())
            hasEnded = true;
        break;

    case CalendarState::Closing:
        hasEnded = true;
        break;
    }

    monthSlide->Step();
    layout->Calculate();
    PlaceCells(cells, ShownGrid);
    if (hasIncomingMonth)
        PlaceCells(incomingCells, incomingGrid);
    return hasEnded;
}

void CalendarScreen::RunChoosing()
{
    FocusTheCellsUnderThePointers();
    if (services.input.AnyPlayerPressed(BoardNextButtons))
    {
        TurnMonth(1);
        return;
    }
    if (services.input.AnyPlayerPressed(BoardPreviousButtons))
    {
        TurnMonth(-1);
        return;
    }

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        if (!player || !place || !HasGoneDown(*player, BoardPressButtons))
            continue;

        BottomBarScreen* bar = Bar();
        std::optional<BarButton> button = bar ? bar->ButtonAt(*place) : std::nullopt;
        if (button == BarButton::BackButton)
        {
            bar->FlashButton(BarButton::BackButton);
            services.sounds.Request(SoundName::Cancel);
            Close(false);
            return;
        }
        if (button == BarButton::NextPage || button == BarButton::PreviousPage)
        {
            TurnMonth(button == BarButton::NextPage ? 1 : -1);
            return;
        }

        for (std::size_t index = 0; index < CalendarCellCount; ++index)
        {
            if (!cells[index]->IsPointedAt(*place))
                continue;

            cells[index]->Select();
            services.sounds.Request(SelectSound);
            data.shownDay = cells[index]->Day();
            chosenCell = index;
            Close(true);
            return;
        }
    }
}

void CalendarScreen::FocusTheCellsUnderThePointers()
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        std::optional<std::size_t> underThePointer;
        for (std::size_t index = 0; place && index < CalendarCellCount; ++index)
        {
            if (cells[index]->IsPointedAt(*place))
                underThePointer = index;
        }

        std::optional<std::size_t>& focused = focusedCells[playerIndex];
        if (underThePointer == focused)
            continue;
        if (focused)
            cells[*focused]->Unfocus();
        if (underThePointer)
        {
            cells[*underThePointer]->Focus();
            services.sounds.Request(FocusSound);
        }
        focused = underThePointer;
    }
}

// The month after slides in from the right, the month before from the left (0x8139E7E0, 0x8139E9B8).
void CalendarScreen::TurnMonth(int direction)
{
    int monthNumber = year * MonthsPerYear + (month - 1) + direction;
    int newYear = monthNumber / MonthsPerYear;
    int newMonth = monthNumber % MonthsPerYear + 1;
    if (newYear < FirstBoardDay.year || newYear > LastBoardDay.year)
        return;

    for (std::optional<std::size_t>& focused : focusedCells)
    {
        if (focused)
            cells[*focused]->Unfocus();
        focused.reset();
    }

    services.sounds.Request(ScrollSound);
    year = newYear;
    month = newMonth;
    incomingGrid = direction > 0 ? NextGrid : PreviousGrid;
    ShowTheMonth(year, month, incomingCells, incomingGrid);
    hasIncomingMonth = true;

    if (direction > 0)
    {
        monthSlide->PlayOnce(FirstFrameToNextMonth, LastFrameToNextMonth);
        state = CalendarState::TurningToNextMonth;
    }
    else
    {
        monthSlide->PlayOnce(FirstFrameToPreviousMonth, LastFrameToPreviousMonth);
        state = CalendarState::TurningToPreviousMonth;
    }
}

void CalendarScreen::Close(bool wasADayChosen)
{
    if (BottomBarScreen* bar = Bar())
        bar->Change(wasADayChosen ? BarChange::CalendarClosesWithADay : BarChange::CalendarClosesWithBack);
    state = wasADayChosen ? CalendarState::ShowingTheChoice : CalendarState::Closing;
}

void CalendarScreen::OnRunEnded()
{
    monthSlide->PlayOnce(FirstFrameOfClosing, LastFrameOfClosing);
}

bool CalendarScreen::RunClosing()
{
    monthSlide->Step();
    layout->Calculate();
    PlaceCells(cells, ShownGrid);
    return !monthSlide->IsPlaying();
}

void CalendarScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !layout)
        return;

    layout->Draw();
    for (std::unique_ptr<CalendarDayView>& cell : cells)
        cell->Draw();
    if (hasIncomingMonth)
    {
        for (std::unique_ptr<CalendarDayView>& cell : incomingCells)
            cell->Draw();
    }
}

}
