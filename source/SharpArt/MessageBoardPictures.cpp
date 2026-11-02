#include "SharpArt/MessageBoardPictures.h"

#include <array>
#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color Gray(Formats::Byte level)
{
    return {level, level, level, 255};
}

constexpr Color White = Gray(255);

void FillRectangle(VectorCanvas& canvas, float left, float top, float right, float bottom, Color color)
{
    VectorPath rectangle;
    rectangle.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(rectangle, SolidPaint(color));
}

void FillRoundedPolygon(VectorCanvas& canvas, const std::vector<Point>& corners, float cornerCut, Color color)
{
    VectorPath polygon;
    polygon.AddPolygonWithRoundCorners(corners, cornerCut);
    canvas.Fill(polygon, SolidPaint(color));
}

// ---- The ball: lit from the upper left, it darkens evenly towards the lower right, where it turns a little blue; a thin bright streak
// runs along the upper left rim. Angles are in degrees, 0 to the right and 90 down.

constexpr Point BallCenter = {32.5f, 32.3f};
constexpr float BallRadius = 31.4f;
constexpr Point LightCenter = {26, 26};
constexpr float LightUpTo = 20;
constexpr float MiddleAt = 27.2f;
constexpr float DarkerAt = 31;
constexpr float DarkestAt = 35;
constexpr Color BallLight = Gray(233);
constexpr Color BallMiddle = Gray(214);
constexpr Color BallDarker = Gray(190);
constexpr Color BallDarkestBlueGray = {173, 181, 181, 255};

constexpr float StreakStartDegrees = 212;
constexpr float StreakEndDegrees = 255;
constexpr float StreakInnerRadius = 26.8f;
constexpr float StreakOuterRadius = 28.4f;
constexpr Color StreakWhite = {255, 255, 255, 90};

void DrawBall(VectorCanvas& canvas)
{
    VectorPath ball;
    ball.AddCircle(BallCenter, BallRadius);
    canvas.Fill(ball, RadialGradientByDistance(LightCenter,
        {{LightUpTo, BallLight}, {MiddleAt, BallMiddle}, {DarkerAt, BallDarker}, {DarkestAt, BallDarkestBlueGray}}));

    VectorPath streak;
    streak.ArcTo(BallCenter, StreakOuterRadius, StreakOuterRadius, StreakStartDegrees, StreakEndDegrees);
    streak.ArcTo(BallCenter, StreakInnerRadius, StreakInnerRadius, StreakEndDegrees, StreakStartDegrees);
    streak.Close();
    canvas.Fill(streak, SolidPaint(StreakWhite));
}

// ---- The calendar: a dark frame around a gray page, four rows of white days with the first two days of the top row and the last day of
// the bottom row shaded, and a plain gray part below the days.

constexpr Color CalendarFrame = Gray(145);
constexpr Color CalendarPage = Gray(190);
constexpr Color ShadedDay = Gray(215);

struct Stretch
{
    float from = 0;
    float to = 0;
};

constexpr std::array CalendarColumns = {
    Stretch{17, 21},
    Stretch{22.5f, 27},
    Stretch{28, 32.5f},
    Stretch{33.8f, 38},
    Stretch{39.5f, 44},
    Stretch{45, 49},
};

constexpr std::array CalendarRows = {
    Stretch{17, 21},
    Stretch{23, 27},
    Stretch{28, 32},
    Stretch{33, 37},
};

bool IsShadedDay(std::size_t row, std::size_t column)
{
    constexpr std::size_t TopRow = 0;
    constexpr std::size_t BottomRow = CalendarRows.size() - 1;
    constexpr std::size_t LastColumn = CalendarColumns.size() - 1;
    bool isFirstTwoOfTheTopRow = row == TopRow && column < 2;
    bool isLastOfTheBottomRow = row == BottomRow && column == LastColumn;
    return isFirstTwoOfTheTopRow || isLastOfTheBottomRow;
}

void DrawCalendar(VectorCanvas& canvas)
{
    FillRectangle(canvas, 15, 15.6f, 50.9f, 48, CalendarFrame);
    FillRectangle(canvas, 16, 17, 50, 47, CalendarPage);

    for (std::size_t row = 0; row < CalendarRows.size(); ++row)
    {
        for (std::size_t column = 0; column < CalendarColumns.size(); ++column)
        {
            Color day = IsShadedDay(row, column) ? ShadedDay : White;
            const Stretch& across = CalendarColumns[column];
            const Stretch& down = CalendarRows[row];
            FillRectangle(canvas, across.from, down.from, across.to, down.to, day);
        }
    }
}

// ---- The memo: a sheet of lined paper whose lower right corner is folded over, and a pencil lying across its upper part.

constexpr Color PaperEdge = Gray(175);
constexpr Color PaperLine = Gray(215);
constexpr Color FoldedCorner = Gray(158);
constexpr Color Pencil = Gray(150);
constexpr float PaperEdgeWidth = 1.2f;
constexpr float FirstPaperLineY = 24;
constexpr float PaperLineSpacing = 6;
constexpr int PaperLineCount = 4;

void DrawMemo(VectorCanvas& canvas)
{
    constexpr float Left = 18;
    constexpr float Top = 16;
    constexpr float Right = 46.5f;
    constexpr float Bottom = 48;
    constexpr Point FoldOnTheSide = {Right, 42.4f};
    constexpr Point FoldAtTheBottom = {35.5f, Bottom};

    constexpr float Edge = PaperEdgeWidth;
    FillRoundedPolygon(canvas, {{Left, Top}, {Right, Top}, FoldOnTheSide, FoldAtTheBottom, {Left, Bottom}}, 0, PaperEdge);
    FillRoundedPolygon(canvas, {{Left + Edge, Top + Edge}, {Right - Edge, Top + Edge}, {Right - Edge, 42.6f}, {35.2f, Bottom - Edge},
        {Left + Edge, Bottom - Edge}}, 0, White);

    for (int line = 0; line < PaperLineCount; ++line)
    {
        float y = FirstPaperLineY + PaperLineSpacing * static_cast<float>(line);
        FillRectangle(canvas, Left + Edge, y, Right - Edge, y + 1, PaperLine);
    }

    FillRoundedPolygon(canvas, {{38.5f, 42.6f}, {Right, 42.6f}, {34.8f, 47.4f}}, 0.5f, FoldedCorner);

    constexpr float PencilCornerCut = 1;
    FillRoundedPolygon(canvas, {{38.2f, 12.2f}, {40.8f, 11.8f}, {43.6f, 14.2f}, {43.4f, 15.6f}, {35.2f, 30}, {33.3f, 32.3f}, {29, 35.4f},
        {28, 34.5f}, {28, 30}}, PencilCornerCut, Pencil);
}

}

void DrawCalendarButton(VectorCanvas& canvas)
{
    DrawBall(canvas);
    DrawCalendar(canvas);
}

void DrawMemoButton(VectorCanvas& canvas)
{
    DrawBall(canvas);
    DrawMemo(canvas);
}

}
