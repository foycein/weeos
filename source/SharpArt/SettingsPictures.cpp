#include "SharpArt/SettingsPictures.h"

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

}
