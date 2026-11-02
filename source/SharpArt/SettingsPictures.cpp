#include "SharpArt/SettingsPictures.h"

#include <array>
#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};
constexpr Color RimBlue = {49, 189, 239, 255};

constexpr Color Gray(Formats::Byte level)
{
    return {level, level, level, 255};
}

// ---- The buttons of the pages

// The pill of a whole button: its white edge, the blue rim inside it and the inside, each a rounded box a few units inside the one before.
constexpr float ButtonTop = 2;
constexpr float ButtonBottom = 62;
constexpr float ButtonCornerRadius = 17;
constexpr float RimInset = 3;
constexpr float InsideInset = 6;

// Where the white edge of the left end begins; the right end is its mirror. A piece only shows the part of the pill that lies on it, so the
// pill reaches far beyond the sides that continue in the middle strip.
constexpr float LeftEndEdge = 8;
constexpr float RightEndEdge = 24;
constexpr float BeyondThePiece = 40;
constexpr float EndWidth = 32;
constexpr float MiddleWidth = 8;

struct PillSides
{
    float left = 0;
    float right = 0;
};

constexpr PillSides LeftEndSides = {LeftEndEdge, EndWidth + BeyondThePiece};
constexpr PillSides MiddleSides = {-BeyondThePiece, MiddleWidth + BeyondThePiece};
constexpr PillSides RightEndSides = {-BeyondThePiece, RightEndEdge};

VectorPath PillInset(const PillSides& sides, float inset)
{
    float left = sides.left + inset;
    float top = ButtonTop + inset;
    VectorPath pill;
    pill.AddRoundedRectangle(left, top, sides.right - inset - left, ButtonBottom - inset - top, ButtonCornerRadius - inset);
    return pill;
}

// The inside is light gray above a gray band across the middle and white below it. The band fades from gray to light gray downward, and
// its ends curve up into the light gray.
constexpr float BandTop = 32;
constexpr float BandBottom = 42;
constexpr float BandEndCurveWidth = 7;
constexpr float BandEndCurveHeight = 8;
constexpr float DownDegrees = 90;
constexpr float LeftDegrees = 180;
constexpr float UpDegrees = 270;

Paint VerticalGradient(const std::vector<ColorStop>& stopsByHeight)
{
    constexpr float Height = 64;
    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = {0, 0};
    paint.end = {0, Height};
    for (const ColorStop& stop : stopsByHeight)
        paint.stops.push_back({stop.share / Height, stop.color});
    return paint;
}

Paint InsidePaint()
{
    return VerticalGradient({{BandTop, Gray(247)}, {BandTop + 0.01f, White}});
}

Paint BandPaint()
{
    return VerticalGradient({{BandTop, Gray(222)}, {33.5f, Gray(222)}, {35, Gray(231)}, {37.5f, Gray(239)}, {40.5f, Gray(247)}});
}

VectorPath BandPath(const PillSides& sides)
{
    float left = sides.left + InsideInset;
    float right = sides.right - InsideInset;
    float curveBottom = BandTop + BandEndCurveHeight;

    VectorPath band;
    band.MoveTo({left, BandBottom});
    band.ArcTo({left + BandEndCurveWidth, curveBottom}, BandEndCurveWidth, BandEndCurveHeight, LeftDegrees, UpDegrees);
    band.ArcTo({right - BandEndCurveWidth, curveBottom}, BandEndCurveWidth, BandEndCurveHeight, UpDegrees, UpDegrees + DownDegrees);
    band.LineTo({right, BandBottom});
    band.Close();
    return band;
}

void DrawButtonPiece(VectorCanvas& canvas, const PillSides& sides)
{
    canvas.Fill(PillInset(sides, 0), SolidPaint(White));
    canvas.Fill(PillInset(sides, RimInset), SolidPaint(RimBlue));
    canvas.Fill(PillInset(sides, InsideInset), InsidePaint());
    canvas.Fill(BandPath(sides), BandPaint());
}

void DrawButtonLightPiece(VectorCanvas& canvas, const PillSides& sides)
{
    canvas.Fill(PillInset(sides, InsideInset), SolidPaint(White));
}

void DrawButtonLightPieceOnBlack(VectorCanvas& canvas, const PillSides& sides)
{
    constexpr Color Black = {0, 0, 0, 255};
    VectorPath whole;
    whole.AddRectangle(0, 0, static_cast<float>(canvas.UnitWidth()), static_cast<float>(canvas.UnitHeight()));
    canvas.Fill(whole, SolidPaint(Black));
    DrawButtonLightPiece(canvas, sides);
}

}

void DrawSettingsPageButtonLeftEnd(VectorCanvas& canvas)
{
    DrawButtonPiece(canvas, LeftEndSides);
}

void DrawSettingsPageButtonMiddle(VectorCanvas& canvas)
{
    DrawButtonPiece(canvas, MiddleSides);
}

void DrawSettingsPageButtonRightEnd(VectorCanvas& canvas)
{
    DrawButtonPiece(canvas, RightEndSides);
}

void DrawSettingsPageButtonLightLeftEnd(VectorCanvas& canvas)
{
    DrawButtonLightPiece(canvas, LeftEndSides);
}

void DrawSettingsPageButtonLightMiddle(VectorCanvas& canvas)
{
    DrawButtonLightPiece(canvas, MiddleSides);
}

void DrawSettingsPageButtonLightRightEnd(VectorCanvas& canvas)
{
    DrawButtonLightPiece(canvas, RightEndSides);
}

void DrawSettingsPageButtonLightOnBlackLeftEnd(VectorCanvas& canvas)
{
    DrawButtonLightPieceOnBlack(canvas, LeftEndSides);
}

void DrawSettingsPageButtonLightOnBlackMiddle(VectorCanvas& canvas)
{
    DrawButtonLightPieceOnBlack(canvas, MiddleSides);
}

void DrawSettingsPageButtonLightOnBlackRightEnd(VectorCanvas& canvas)
{
    DrawButtonLightPieceOnBlack(canvas, RightEndSides);
}

// ---- The arrow buttons that scroll a page up and down

namespace
{

// A box with a thick black edge, white at the top and gray at the bottom, around a black arrow whose base is notched towards its point.
// The places are those of the pages of Wii Settings (72 by 72); the keyboard's picture (72 by 52) is the same box without the empty rows
// above it.
constexpr Color Black = {0, 0, 0, 255};
constexpr float BoxTop = 10;
constexpr float BoxBottom = 62;
constexpr float BoxWidth = 72;
constexpr float BoxCornerRadius = 11;
constexpr float BoxEdgeWidth = 3;
constexpr float KeyboardBoxShift = -10;

// The arrow that points up; the arrow that points down is its mirror around the middle of the box.
constexpr Point ArrowPoint = {36, 21.3f};
constexpr Point ArrowBaseLeft = {17.3f, 48};
constexpr Point ArrowBaseRight = {54.7f, 48};
constexpr Point ArrowBaseMiddle = {36, 43.2f};

Point MirroredAcrossTheBox(Point point, bool pointsUp)
{
    if (pointsUp)
        return point;

    float boxMiddle = (BoxTop + BoxBottom) / 2;
    return {point.x, 2 * boxMiddle - point.y};
}

void DrawArrowButton(VectorCanvas& canvas, float shift, bool pointsUp)
{
    VectorPath box;
    box.AddRoundedRectangle(0, BoxTop + shift, BoxWidth, BoxBottom - BoxTop, BoxCornerRadius);
    canvas.Fill(box, SolidPaint(Black));

    float insideTop = BoxTop + BoxEdgeWidth + shift;
    float insideBottom = BoxBottom - BoxEdgeWidth + shift;
    VectorPath inside;
    inside.AddRoundedRectangle(BoxEdgeWidth, insideTop, BoxWidth - 2 * BoxEdgeWidth, insideBottom - insideTop, BoxCornerRadius - BoxEdgeWidth);
    canvas.Fill(inside, LinearGradient({0, insideTop}, Gray(254), {0, insideBottom}, Gray(185)));

    auto place = [shift, pointsUp](Point point)
    {
        Point mirrored = MirroredAcrossTheBox(point, pointsUp);
        return Point{mirrored.x, mirrored.y + shift};
    };

    VectorPath arrow;
    arrow.AddPolygon({place(ArrowPoint), place(ArrowBaseRight), place(ArrowBaseMiddle), place(ArrowBaseLeft)});
    canvas.Fill(arrow, SolidPaint(Black));
}

}

void DrawSettingsPageArrowUp(VectorCanvas& canvas)
{
    DrawArrowButton(canvas, 0, true);
}

void DrawSettingsPageArrowDown(VectorCanvas& canvas)
{
    DrawArrowButton(canvas, 0, false);
}

void DrawKeyboardArrowUp(VectorCanvas& canvas)
{
    DrawArrowButton(canvas, KeyboardBoxShift, true);
}

void DrawKeyboardArrowDown(VectorCanvas& canvas)
{
    DrawArrowButton(canvas, KeyboardBoxShift, false);
}

// ---- The big square button of the index pages of Wii Settings and of Data Management

namespace
{

// A white edge, a blue rim and the inside: light gray at the top, a darker band that fades to white, and a soft shadow along the right
// side and the bottom.
constexpr float FrameLeft = 1;
constexpr float FrameTop = 1;
constexpr float FrameRight = 142.8f;
constexpr float FrameBottom = 112;
constexpr float FrameCornerRadius = 8;
constexpr float FrameRimInset = 2.85f;
constexpr float FrameInsideInset = 4.9f;
constexpr float RightShadowStart = 129;
constexpr float BottomShadowStart = 97;
constexpr Color FrameRimBlue = {74, 189, 239, 255};

VectorPath FrameInset(float inset)
{
    float left = FrameLeft + inset;
    float top = FrameTop + inset;
    float right = FrameRight - inset;
    float bottom = FrameBottom - inset;
    VectorPath frame;
    frame.AddRoundedRectangle(left, top, right - left, bottom - top, FrameCornerRadius - inset);
    return frame;
}

Paint FrameInsidePaint()
{
    return VerticalGradient({
        {11.7f, Gray(222)},
        {12.2f, Gray(214)},
        {21.5f, Gray(214)},
        {23, Gray(189)},
        {27.5f, Gray(189)},
        {28.5f, Gray(198)},
        {31, Gray(206)},
        {33.5f, Gray(214)},
        {35.5f, Gray(222)},
        {38, Gray(231)},
        {40.5f, Gray(239)},
        {43.5f, Gray(247)},
        {46, White},
    });
}

// A shadow inside the rim, from nothing at the start to its darkest at the inside's edge; its rounded corners follow those of the inside.
void FillShadow(VectorCanvas& canvas, Point start, Point end, Formats::Byte darkestAlpha)
{
    constexpr float InsideCornerRadius = FrameCornerRadius - FrameInsideInset;
    float right = FrameRight - FrameInsideInset;
    float bottom = FrameBottom - FrameInsideInset;

    VectorPath shadow;
    shadow.AddRoundedRectangle(start.x, start.y, right - start.x, bottom - start.y, InsideCornerRadius);
    canvas.Fill(shadow, LinearGradient(start, {0, 0, 0, 0}, end, {0, 0, 0, darkestAlpha}));
}

}

void DrawBigSettingsButton(VectorCanvas& canvas)
{
    constexpr Formats::Byte RightShadowAlpha = 57;
    constexpr Formats::Byte BottomShadowAlpha = 90;
    float insideLeft = FrameLeft + FrameInsideInset;
    float insideTop = FrameTop + FrameInsideInset;
    float insideRight = FrameRight - FrameInsideInset;
    float insideBottom = FrameBottom - FrameInsideInset;

    canvas.Fill(FrameInset(0), SolidPaint(White));
    canvas.Fill(FrameInset(FrameRimInset), SolidPaint(FrameRimBlue));
    canvas.Fill(FrameInset(FrameInsideInset), FrameInsidePaint());
    FillShadow(canvas, {RightShadowStart, insideTop}, {insideRight, insideTop}, RightShadowAlpha);
    FillShadow(canvas, {insideLeft, BottomShadowStart}, {insideLeft, insideBottom}, BottomShadowAlpha);
}

// ---- The strength of the signal on the page of the connection test

namespace
{

// A dark rounded square with a colored band at the top and the bottom, and between them a flag on a pole and three bars that grow to the
// right; the bars that the signal reaches are dark, the others light gray.
constexpr Color SignalDark = {33, 24, 16, 255};
constexpr Color UnlitBarGray = Gray(206);
constexpr Color LineBelowTheTopBand = {231, 231, 200, 255};
constexpr Color WeakRed = {255, 0, 0, 255};
constexpr Color MiddleOrange = {255, 173, 0, 255};
constexpr Color StrongGreen = {24, 231, 41, 255};
constexpr float IconSize = 40;
constexpr float IconCornerRadius = 7;
constexpr float IconEdgeWidth = 1;
constexpr float IconBottomEdgeWidth = 0.4f;
constexpr float TopBandBottom = 7;
constexpr float BottomBandTop = 32.6f;
constexpr float BarBottom = 29;
constexpr float BarWidth = 4;

struct Bar
{
    float left = 0;
    float top = 0;
};

constexpr std::array SignalBars = {Bar{17, 20}, Bar{24, 15}, Bar{31, 10}};

void FillBox(VectorCanvas& canvas, float left, float top, float right, float bottom, const Paint& paint)
{
    VectorPath box;
    box.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(box, paint);
}

void DrawSignalIcon(VectorCanvas& canvas, Color bandColor, std::size_t litBarCount)
{
    constexpr float InsideWidth = IconSize - 2 * IconEdgeWidth;
    constexpr float InsideHeight = IconSize - IconEdgeWidth - IconBottomEdgeWidth;
    constexpr float InsideRight = IconSize - IconEdgeWidth;

    VectorPath icon;
    icon.AddRoundedRectangle(0, 0, IconSize, IconSize, IconCornerRadius);
    canvas.Fill(icon, SolidPaint(SignalDark));

    VectorPath bands;
    bands.AddRoundedRectangle(IconEdgeWidth, IconEdgeWidth, InsideWidth, InsideHeight, IconCornerRadius - IconEdgeWidth);
    canvas.Fill(bands, SolidPaint(bandColor));

    Paint whiteFading = VerticalGradient({{11.5f, White}, {BottomBandTop - 1, Gray(212)}});
    FillBox(canvas, IconEdgeWidth, TopBandBottom, InsideRight, BottomBandTop, whiteFading);
    FillBox(canvas, IconEdgeWidth, TopBandBottom, InsideRight, TopBandBottom + 1, SolidPaint(LineBelowTheTopBand));

    constexpr float FlagLeft = 5;
    constexpr float FlagTop = 10;
    constexpr float FlagSize = 9;
    constexpr float FlagCornerRadius = 0.8f;
    VectorPath flag;
    flag.AddRoundedRectangle(FlagLeft, FlagTop, FlagSize, FlagSize, FlagCornerRadius);
    canvas.Fill(flag, SolidPaint(SignalDark));
    FillBox(canvas, 5.5f, 11, 13.5f, 18.5f, SolidPaint(bandColor));
    FillBox(canvas, 8.1f, FlagTop + FlagSize, 11, BarBottom, SolidPaint(SignalDark));

    for (std::size_t index = 0; index < SignalBars.size(); ++index)
    {
        Color barColor = index < litBarCount ? SignalDark : UnlitBarGray;
        FillBox(canvas, SignalBars[index].left, SignalBars[index].top, SignalBars[index].left + BarWidth, BarBottom, SolidPaint(barColor));
    }
}

}

void DrawSignalNone(VectorCanvas& canvas)
{
    DrawSignalIcon(canvas, WeakRed, 0);
}

void DrawSignalWeak(VectorCanvas& canvas)
{
    DrawSignalIcon(canvas, MiddleOrange, 1);
}

void DrawSignalMiddle(VectorCanvas& canvas)
{
    DrawSignalIcon(canvas, StrongGreen, 2);
}

void DrawSignalStrong(VectorCanvas& canvas)
{
    DrawSignalIcon(canvas, StrongGreen, 3);
}

}
