#pragma once

#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/MessageBoard/BoardDays.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/ScreenId.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>

namespace SystemMenu
{

class BottomBarScreen;
struct MessageBoardData;

// One day of the calendar (the layout my_IplTop_f): its number, the frame that marks today, the color of its weekday, the flash of a
// choice and the mark of a day with letters. The five animations play parts of one file on five panes (0x8139F8CC, the table 0x8160FBA0).
class CalendarDayView
{
public:
    CalendarDayView(Display& display, const Layout::LayoutArchive& archive);

    void ShowDay(BoardDay day, bool isInTheMonth, bool isToday, bool hasLetters, const std::u16string& number);

    BoardDay Day() const;
    bool IsInTheMonth() const;
    bool IsPointedAt(Formats::Vector2 place);
    std::optional<Formats::Vector2> PlaceOfButton();
    void Focus();
    void Unfocus();
    void Select();
    bool IsSelecting() const;
    void PutAt(Formats::Vector2 place);
    void Calculate();
    void Draw();

private:
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<FramePlayer> focusPart;
    std::unique_ptr<FramePlayer> todayPart;
    std::unique_ptr<FramePlayer> weekdayPart;
    std::unique_ptr<FramePlayer> selectPart;
    std::unique_ptr<FramePlayer> markPart;
    BoardDay day;
    bool isInTheMonth = false;
};

// The calendar of the Message Board (screen 0x07, 0x8139DBB0 to 0x8139F8C4). It shows one month, Sunday first; days with a letter or a
// memo carry a mark. The arrows of the bar, Plus and Minus turn the months from January 2000 to December 2035. A day of the month that
// is pressed becomes the day of the board; Back closes the calendar.
class CalendarScreen : public Screen
{
public:
    explicit CalendarScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    int ShownYear() const;
    int ShownMonth() const;

    // The cells of the month that shows, for the checks of the tests: 42 days, Sunday first.
    const std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount>& Cells() const;

protected:
    bool IsReady() override;
    bool RunMain() override;
    void OnRunEnded() override;
    bool RunClosing() override;

private:
    enum class CalendarState
    {
        Opening,
        Choosing,
        TurningToNextMonth,
        TurningToPreviousMonth,
        ShowingTheChoice,
        Closing,
    };

    void ShowTheMonth(int year, int month, std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount>& cells, std::string_view gridLetter);
    void PlaceCells(std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount>& cells, std::string_view gridLetter);
    void ShowTheArrows();
    void RunChoosing();
    void FocusTheCellsUnderThePointers();
    void TurnMonth(int direction);
    void StepTheMonthSlide();
    void Close(bool wasADayChosen);
    BottomBarScreen* Bar();

    MenuServices& services;
    MessageBoardData& data;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<FramePlayer> monthSlide;
    std::unique_ptr<FramePlayer> weekendColors;
    std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount> cells;
    std::array<std::unique_ptr<CalendarDayView>, CalendarCellCount> incomingCells;
    std::array<std::optional<std::size_t>, PlayerCount> focusedCells;
    CalendarState state = CalendarState::Opening;
    int year = 2000;
    int month = 1;
    std::size_t chosenCell = 0;
    bool hasIncomingMonth = false;
    std::string_view incomingGrid;
};

}
