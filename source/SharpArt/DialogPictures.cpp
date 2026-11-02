#include "SharpArt/DialogPictures.h"

#include <vector>

namespace SharpArt
{

namespace
{

Color Gray(Formats::Byte level)
{
    return {level, level, level, 255};
}

Color ClearGray(Formats::Byte level)
{
    return {level, level, level, 0};
}

Paint GradientAlong(Point start, Point direction, const std::vector<ColorStop>& stopsByDistance)
{
    float length = stopsByDistance.back().share;

    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = start;
    paint.end = {start.x + direction.x * length, start.y + direction.y * length};
    for (const ColorStop& stop : stopsByDistance)
        paint.stops.push_back({stop.share / length, stop.color});
    return paint;
}

// ---- The answer button

// Each end is half a circle around a point on its seam with the middle strip: a thin black rim, a gray body that darkens towards its
// lower and outer edge, a light band along the upper edge of the left end and a white gloss in the upper half.
constexpr float MiddleY = 40;
constexpr float RimRadius = 31;
constexpr float BodyRadius = 30.4f;
constexpr float GlossRadius = 27.6f;
constexpr float GlossTop = MiddleY - GlossRadius;
constexpr float GlossBottomAtTheSeams = 32.3f;
constexpr float LeftEndCenterX = 32.41f;
constexpr float RightEndCenterX = 0.55f;
constexpr Color Black = {0, 0, 0, 255};
constexpr Color White = {255, 255, 255, 255};

// The body darkens with the distance from the middle line of the button: straight out to the side in the upper half, and around the
// center of the end in the lower half.
const std::vector<ColorStop> ShadeByDistance = {
    {17, Gray(156)},
    {19, Gray(148)},
    {21, Gray(132)},
    {23, Gray(107)},
    {25, Gray(74)},
    {27, Gray(41)},
    {29, Gray(24)},
    {31, Gray(8)},
};

// The light band of the left end changes from top to bottom and fades out low on the left side.
const std::vector<ColorStop> LeftBandByHeight = {
    {12, Gray(206)},
    {20, Gray(214)},
    {30, Gray(189)},
    {36, Gray(173)},
    {40, Gray(165)},
    {44, Gray(132)},
    {48, Gray(99)},
    {52, Gray(70)},
    {58, ClearGray(41)},
};

// On the right end the band only runs along the top.
const std::vector<ColorStop> RightBandByDegrees = {
    {0, ClearGray(189)},
    {90, ClearGray(189)},
    {270, Gray(189)},
    {290, Gray(189)},
    {320, ClearGray(165)},
};

void FillHalfOfCircle(VectorCanvas& canvas, Point center, float radius, float startDegrees, const Paint& paint)
{
    constexpr float HalfTurnDegrees = 180;
    VectorPath half;
    half.ArcTo(center, radius, radius, startDegrees, startDegrees + HalfTurnDegrees);
    half.Close();
    canvas.Fill(half, paint);
}

// The body of an end. The lower half darkens around the center; the upper half of the right end darkens towards its side, while the
// upper half of the left end, which the light falls on, keeps its gray.
void DrawEndBody(VectorCanvas& canvas, float centerX, const Paint& upperHalfPaint)
{
    constexpr float LowerHalfStart = 0;
    constexpr float UpperHalfStart = 180;
    Point center = {centerX, MiddleY};

    VectorPath rim;
    rim.AddCircle(center, RimRadius);
    canvas.Fill(rim, SolidPaint(Black));

    FillHalfOfCircle(canvas, center, BodyRadius, UpperHalfStart, upperHalfPaint);
    FillHalfOfCircle(canvas, center, BodyRadius, LowerHalfStart, RadialGradientByDistance(center, ShadeByDistance));
}

void DrawEndBand(VectorCanvas& canvas, float centerX, const Paint& paint)
{
    VectorPath band;
    band.AddCircle({centerX, MiddleY}, BodyRadius);
    band.AddCircle({centerX, MiddleY}, GlossRadius);
    canvas.Fill(band, paint);
}

// The gloss of the left end ends low on the left side in a tip that curls inwards.
void DrawLeftEndGloss(VectorCanvas& canvas)
{
    constexpr float SeamX = 32;
    constexpr float LeftDegrees = 180;
    Point center = {LeftEndCenterX, MiddleY};

    VectorPath gloss;
    gloss.MoveTo({SeamX, GlossBottomAtTheSeams});
    gloss.CurveTo({23, GlossBottomAtTheSeams}, {16, 35}, {13, 41.5f});
    gloss.CurveTo({11.8f, 44}, {11.3f, 47.5f}, {9.8f, 47.4f});
    gloss.CurveTo({8, 47.3f}, {5.2f, 43}, {center.x - GlossRadius, MiddleY});
    gloss.ArcTo(center, GlossRadius, GlossRadius, LeftDegrees, 270);
    gloss.LineTo({SeamX, GlossTop});
    gloss.Close();
    canvas.Fill(gloss, SolidPaint(White));
}

void DrawRightEndGloss(VectorCanvas& canvas)
{
    constexpr float SeamX = 0;
    constexpr float SideX = 13.8f;

    VectorPath gloss;
    gloss.MoveTo({SeamX, GlossBottomAtTheSeams});
    gloss.CurveTo({6, GlossBottomAtTheSeams}, {SideX, 31}, {SideX, 25});
    gloss.CurveTo({SideX, 16}, {11, GlossTop}, {6, GlossTop});
    gloss.LineTo({SeamX, GlossTop});
    gloss.Close();
    canvas.Fill(gloss, SolidPaint(White));
}

void FillRectangle(VectorCanvas& canvas, float left, float top, float right, float bottom, const Paint& paint)
{
    VectorPath rectangle;
    rectangle.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(rectangle, paint);
}

// ---- The dialog window

constexpr float WindowCornerX = 16.22f;
constexpr float WindowRimRadius = 15.21f;
constexpr float WindowInsideRadius = 13.9f;
constexpr float WindowPieceWidth = 16;
constexpr float WindowPieceHeight = 32;

// The inside is white and turns light gray in a few rows, starting this far below the top of the window's rounded piece.
const std::vector<ColorStop> WindowShadeBelowTheTurn = {
    {0, Gray(255)},
    {1, Gray(238)},
    {2.5f, Gray(221)},
    {4.5f, Gray(204)},
    {7, Gray(187)},
};

Paint WindowShade(float turnY)
{
    return GradientAlong({0, turnY}, {0, 1}, WindowShadeBelowTheTurn);
}

// A rounded corner reaching to the right edge of the piece: the circle around the corner point and everything right of it between the
// two heights.
void FillWindowCorner(VectorCanvas& canvas, float cornerY, float radius, float top, float bottom, const Paint& paint)
{
    VectorPath corner;
    corner.AddCircle({WindowCornerX, cornerY}, radius);
    canvas.Fill(corner, paint);
    FillRectangle(canvas, WindowCornerX, top, WindowPieceWidth, bottom, paint);
}

}

void DrawDialogWindowTopCorner(VectorCanvas& canvas)
{
    constexpr float CornerY = 30.23f;
    constexpr float TurnY = 22.5f;
    float rimTop = CornerY - WindowRimRadius;
    float insideTop = CornerY - WindowInsideRadius;

    FillWindowCorner(canvas, CornerY, WindowRimRadius, rimTop, WindowPieceHeight, SolidPaint(Black));
    FillWindowCorner(canvas, CornerY, WindowInsideRadius, insideTop, WindowPieceHeight, WindowShade(TurnY));
}

void DrawDialogWindowBottomCorner(VectorCanvas& canvas)
{
    constexpr float CornerY = 1.77f;
    constexpr float TurnY = 6.5f;
    float rimBottom = CornerY + WindowRimRadius;
    float insideBottom = CornerY + WindowInsideRadius;

    FillWindowCorner(canvas, CornerY, WindowRimRadius, 0, rimBottom, SolidPaint(Black));
    FillWindowCorner(canvas, CornerY, WindowInsideRadius, 0, insideBottom, WindowShade(TurnY));
}

void DrawDialogWindowMiddle(VectorCanvas& canvas)
{
    constexpr float LineHeight = 1;
    constexpr float LineSpacing = 4;
    float height = static_cast<float>(canvas.UnitHeight());
    for (float top = 0; top < height; top += LineSpacing)
        FillRectangle(canvas, 0, top, WindowPieceWidth, top + LineHeight, SolidPaint(White));
}

void DrawAnswerButtonLeftEnd(VectorCanvas& canvas)
{
    DrawEndBody(canvas, LeftEndCenterX, SolidPaint(Gray(156)));
    DrawEndBand(canvas, LeftEndCenterX, GradientAlong({0, 0}, {0, 1}, LeftBandByHeight));
    DrawLeftEndGloss(canvas);
}

void DrawAnswerButtonMiddle(VectorCanvas& canvas)
{
    float width = static_cast<float>(canvas.UnitWidth());
    FillRectangle(canvas, 0, MiddleY - RimRadius, width, MiddleY + RimRadius, SolidPaint(Black));
    FillRectangle(canvas, 0, MiddleY - BodyRadius, width, MiddleY, SolidPaint(Gray(156)));
    FillRectangle(canvas, 0, MiddleY, width, MiddleY + BodyRadius, GradientAlong({0, MiddleY}, {0, 1}, ShadeByDistance));
    FillRectangle(canvas, 0, MiddleY - BodyRadius, width, GlossTop, GradientAlong({0, 0}, {0, 1}, LeftBandByHeight));
    FillRectangle(canvas, 0, GlossTop, width, GlossBottomAtTheSeams, SolidPaint(White));
}

void DrawAnswerButtonRightEnd(VectorCanvas& canvas)
{
    constexpr Point TowardsTheRight = {1, 0};
    DrawEndBody(canvas, RightEndCenterX, GradientAlong({RightEndCenterX, MiddleY}, TowardsTheRight, ShadeByDistance));
    DrawEndBand(canvas, RightEndCenterX, ConicGradientByDegrees({RightEndCenterX, MiddleY}, RightBandByDegrees));
    DrawRightEndGloss(canvas);
}

}
