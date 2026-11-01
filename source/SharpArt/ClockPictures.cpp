#include "SharpArt/ClockPictures.h"

#include <set>
#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};

// The digits are made of seven bars, like the digits of a pocket calculator.
enum class Bar
{
    Top,
    UpperLeft,
    UpperRight,
    Middle,
    LowerLeft,
    LowerRight,
    Bottom,
};

using LitBars = std::set<Bar>;

// Where the bars lie in the 48 by 48 picture of a digit: the middle lines of the three rows and the two columns.
constexpr float TopRowY = 7.5f;
constexpr float MiddleRowY = 24;
constexpr float BottomRowY = 40.5f;
constexpr float LeftColumnX = 14.25f;
constexpr float RightColumnX = 33.75f;

// The one and the colon stand in the middle of the picture.
constexpr float MiddleColumnX = 24;

constexpr float HalfThickness = 2.75f;

// The corners of the bars are a little round. The pointed ends are drawn this much longer, so their round tips end where the points would.
constexpr float CornerCut = 0.9f;
constexpr float RoundTipLength = 0.4f;

// Every bar is a long hexagon with pointed ends. An end that meets another bar stops this far from the middle line of that bar, so a
// thin gap stays between them.
constexpr float GapAtRowEnd = 0.85f;
constexpr float GapAtColumnEndInTheMiddle = 0.7f;
constexpr float GapAtColumnEndAtTheEdge = 1.15f;

// An end that meets no other bar reaches further out: a column to the outer edge of the missing row, and in the two, three and five a
// row a little beyond the middle line of the missing column.
constexpr float FreeRowEndBeyondColumn = 1.1f;
constexpr float TopEdgeY = TopRowY - HalfThickness;
constexpr float BottomEdgeY = BottomRowY + HalfThickness;

enum class Direction
{
    Across,
    Down,
};

// One bar, from the tip at its start to the tip at its end along its direction, around its middle line.
struct BarShape
{
    Direction direction = Direction::Across;
    float middleLine = 0;
    float start = 0;
    float end = 0;
};

// A point given along the bar and beside its middle line.
Point PlaceOnBar(const BarShape& bar, float along, float beside)
{
    if (bar.direction == Direction::Across)
        return {along, bar.middleLine + beside};
    return {bar.middleLine + beside, along};
}

void AddBar(VectorPath& path, const BarShape& bar)
{
    float start = bar.start - RoundTipLength;
    float end = bar.end + RoundTipLength;
    std::vector<Point> corners = {
        PlaceOnBar(bar, start, 0),
        PlaceOnBar(bar, start + HalfThickness, -HalfThickness),
        PlaceOnBar(bar, end - HalfThickness, -HalfThickness),
        PlaceOnBar(bar, end, 0),
        PlaceOnBar(bar, end - HalfThickness, HalfThickness),
        PlaceOnBar(bar, start + HalfThickness, HalfThickness),
    };
    path.AddPolygonWithRoundCorners(corners, CornerCut);
}

// A row reaches from the left column to the right one.
enum class FreeRowEnds
{
    ReachOut,
    StayIn,
};

BarShape RowBar(float rowY, bool meetsLeftColumn, bool meetsRightColumn, FreeRowEnds freeRowEnds)
{
    BarShape bar{Direction::Across, rowY, LeftColumnX + GapAtRowEnd, RightColumnX - GapAtRowEnd};
    if (freeRowEnds == FreeRowEnds::StayIn)
        return bar;

    if (!meetsLeftColumn)
        bar.start = LeftColumnX - FreeRowEndBeyondColumn;
    if (!meetsRightColumn)
        bar.end = RightColumnX + FreeRowEndBeyondColumn;
    return bar;
}

BarShape UpperColumnBar(float columnX, bool meetsTopRow)
{
    BarShape bar{Direction::Down, columnX, TopEdgeY, MiddleRowY - GapAtColumnEndInTheMiddle};
    if (meetsTopRow)
        bar.start = TopRowY + GapAtColumnEndAtTheEdge;
    return bar;
}

BarShape LowerColumnBar(float columnX, bool meetsBottomRow)
{
    BarShape bar{Direction::Down, columnX, MiddleRowY + GapAtColumnEndInTheMiddle, BottomEdgeY};
    if (meetsBottomRow)
        bar.end = BottomRowY - GapAtColumnEndAtTheEdge;
    return bar;
}

// The ends in the middle of the digit always count as meeting another bar.
BarShape ShapeOfBar(const LitBars& litBars, Bar bar, FreeRowEnds freeRowEnds)
{
    switch (bar)
    {
    case Bar::Top:
        return RowBar(TopRowY, litBars.contains(Bar::UpperLeft), litBars.contains(Bar::UpperRight), freeRowEnds);
    case Bar::Middle:
        return RowBar(MiddleRowY, true, true, freeRowEnds);
    case Bar::Bottom:
        return RowBar(BottomRowY, litBars.contains(Bar::LowerLeft), litBars.contains(Bar::LowerRight), freeRowEnds);
    case Bar::UpperLeft:
        return UpperColumnBar(LeftColumnX, litBars.contains(Bar::Top));
    case Bar::UpperRight:
        return UpperColumnBar(RightColumnX, litBars.contains(Bar::Top));
    case Bar::LowerLeft:
        return LowerColumnBar(LeftColumnX, litBars.contains(Bar::Bottom));
    case Bar::LowerRight:
        return LowerColumnBar(RightColumnX, litBars.contains(Bar::Bottom));
    }
    return {};
}

void DrawDigit(VectorCanvas& canvas, const LitBars& litBars, FreeRowEnds freeRowEnds = FreeRowEnds::StayIn)
{
    VectorPath path;
    for (Bar bar : litBars)
        AddBar(path, ShapeOfBar(litBars, bar, freeRowEnds));
    canvas.Fill(path, SolidPaint(White));
}

// The letters AM and PM are drawn with a round pen of this width.
constexpr float LetterPenWidth = 3.1f;

void FillStroke(VectorCanvas& canvas, Point from, Point to)
{
    VectorPath stroke;
    stroke.AddStroke(from, to, LetterPenWidth);
    canvas.Fill(stroke, SolidPaint(White));
}

void DrawLetterM(VectorCanvas& canvas)
{
    constexpr float Top = 9.95f;
    constexpr float Bottom = 23.05f;
    constexpr float LeftX = 25.45f;
    constexpr float RightX = 36.55f;
    constexpr Point Valley = {31, 20};

    FillStroke(canvas, {LeftX, Top}, {LeftX, Bottom});
    FillStroke(canvas, {RightX, Top}, {RightX, Bottom});
    FillStroke(canvas, {LeftX, Top}, Valley);
    FillStroke(canvas, {RightX, Top}, Valley);
}

// The A: two legs that lean against each other under a narrow top, a small triangle cut out above the cross bar, the opening
// between the legs below it, and round feet.
void DrawLetterA(VectorCanvas& canvas)
{
    constexpr float MiddleX = 14.5f;
    constexpr float TopY = 8.1f;
    constexpr float TopHalfWidth = 1.4f;
    constexpr float FeetY = 24.3f;
    constexpr float OuterFootX = 7.1f;
    constexpr float LegWidth = 3.4f;
    constexpr float FootRadius = 1.55f;
    constexpr float UnderCrossBarY = 19.2f;
    constexpr float HoleTopY = 12.7f;
    constexpr float HoleBottomY = 16.1f;
    constexpr float HoleHalfWidth = 1.3f;

    constexpr float LegSlope = (MiddleX - TopHalfWidth - OuterFootX) / (FeetY - TopY);
    constexpr float FootMiddleY = FeetY - FootRadius;
    constexpr float OuterLegXAtFootMiddle = OuterFootX + LegSlope * FootRadius;
    constexpr float InnerLegXAtFootMiddle = OuterLegXAtFootMiddle + LegWidth;
    constexpr float InnerLegXUnderCrossBar = InnerLegXAtFootMiddle + LegSlope * (FootMiddleY - UnderCrossBarY);
    constexpr float LeftFootX = OuterLegXAtFootMiddle + LegWidth / 2;
    constexpr float RightFootX = 2 * MiddleX - LeftFootX;

    VectorPath letter;
    letter.MoveTo({MiddleX - TopHalfWidth, TopY});
    letter.LineTo({MiddleX + TopHalfWidth, TopY});
    letter.LineTo({2 * MiddleX - OuterLegXAtFootMiddle, FootMiddleY});
    letter.LineTo({2 * MiddleX - InnerLegXAtFootMiddle, FootMiddleY});
    letter.LineTo({2 * MiddleX - InnerLegXUnderCrossBar, UnderCrossBarY});
    letter.LineTo({InnerLegXUnderCrossBar, UnderCrossBarY});
    letter.LineTo({InnerLegXAtFootMiddle, FootMiddleY});
    letter.LineTo({OuterLegXAtFootMiddle, FootMiddleY});
    letter.Close();
    letter.AddPolygon({{MiddleX, HoleTopY}, {MiddleX + HoleHalfWidth, HoleBottomY}, {MiddleX - HoleHalfWidth, HoleBottomY}});
    canvas.Fill(letter, SolidPaint(White));

    VectorPath feet;
    feet.AddCircle({LeftFootX, FootMiddleY}, FootRadius);
    feet.AddCircle({RightFootX, FootMiddleY}, FootRadius);
    canvas.Fill(feet, SolidPaint(White));
}

// The P: a stem, and a bowl of two bars and a half ring, all drawn with the same pen.
void DrawLetterP(VectorCanvas& canvas)
{
    constexpr float StemX = 10.73f;
    constexpr float StemTop = 9.4f;
    constexpr float StemBottom = 22.5f;
    constexpr float BowlBottom = 16.4f;
    constexpr float BowlRadius = (BowlBottom - StemTop) / 2;
    constexpr Point BowlMiddle = {15.63f, StemTop + BowlRadius};
    constexpr float HalfPen = LetterPenWidth / 2;
    constexpr float Up = -90;
    constexpr float Down = 90;

    FillStroke(canvas, {StemX, StemTop}, {StemX, StemBottom});
    FillStroke(canvas, {StemX, StemTop}, {BowlMiddle.x, StemTop});
    FillStroke(canvas, {StemX, BowlBottom}, {BowlMiddle.x, BowlBottom});

    VectorPath halfRing;
    halfRing.ArcTo(BowlMiddle, BowlRadius + HalfPen, BowlRadius + HalfPen, Up, Down);
    halfRing.ArcTo(BowlMiddle, BowlRadius - HalfPen, BowlRadius - HalfPen, Down, Up);
    halfRing.Close();
    canvas.Fill(halfRing, SolidPaint(White));
}

}

void DrawClockDigitZero(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperLeft, Bar::UpperRight, Bar::LowerLeft, Bar::LowerRight, Bar::Bottom});
}

// The one is a single column in the middle.
void DrawClockDigitOne(VectorCanvas& canvas)
{
    VectorPath path;
    AddBar(path, UpperColumnBar(MiddleColumnX, false));
    AddBar(path, LowerColumnBar(MiddleColumnX, false));
    canvas.Fill(path, SolidPaint(White));
}

void DrawClockDigitTwo(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperRight, Bar::Middle, Bar::LowerLeft, Bar::Bottom}, FreeRowEnds::ReachOut);
}

void DrawClockDigitThree(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperRight, Bar::Middle, Bar::LowerRight, Bar::Bottom}, FreeRowEnds::ReachOut);
}

void DrawClockDigitFour(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::UpperLeft, Bar::UpperRight, Bar::Middle, Bar::LowerRight});
}

void DrawClockDigitFive(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperLeft, Bar::Middle, Bar::LowerRight, Bar::Bottom}, FreeRowEnds::ReachOut);
}

void DrawClockDigitSix(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperLeft, Bar::Middle, Bar::LowerLeft, Bar::LowerRight, Bar::Bottom});
}

// The seven of the Wii has the upper left bar too.
void DrawClockDigitSeven(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperLeft, Bar::UpperRight, Bar::LowerRight});
}

void DrawClockDigitEight(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperLeft, Bar::UpperRight, Bar::Middle, Bar::LowerLeft, Bar::LowerRight, Bar::Bottom});
}

void DrawClockDigitNine(VectorCanvas& canvas)
{
    DrawDigit(canvas, {Bar::Top, Bar::UpperLeft, Bar::UpperRight, Bar::Middle, Bar::LowerRight, Bar::Bottom});
}

void DrawClockColon(VectorCanvas& canvas)
{
    constexpr float DotRadius = 3.1f;
    constexpr float UpperDotY = 16;
    constexpr float LowerDotY = 32;

    VectorPath dots;
    dots.AddCircle({MiddleColumnX, UpperDotY}, DotRadius);
    dots.AddCircle({MiddleColumnX, LowerDotY}, DotRadius);
    canvas.Fill(dots, SolidPaint(White));
}

void DrawClockAm(VectorCanvas& canvas)
{
    DrawLetterA(canvas);
    DrawLetterM(canvas);
}

void DrawClockPm(VectorCanvas& canvas)
{
    DrawLetterP(canvas);
    DrawLetterM(canvas);
}

}
