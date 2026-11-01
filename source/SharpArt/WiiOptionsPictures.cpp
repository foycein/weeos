#include "SharpArt/WiiOptionsPictures.h"

#include <vector>

namespace SharpArt
{

namespace
{

// The frame of a big button of Wii Options (256 by 200): a white edge, a blue rim, and inside a gray band at the top that fades to
// white, a thin line above the caption, and a soft shadow along the right side and the bottom.
constexpr Color White = {255, 255, 255, 255};
constexpr Color RimBlue = {74, 189, 239, 255};
constexpr float FrameLeft = 3;
constexpr float FrameTop = 2;
constexpr float FrameRight = 252;
constexpr float FrameBottom = 197;
constexpr float FrameCornerRadius = 7;
constexpr float RimLeft = 6;
constexpr float RimTop = 5;
constexpr float RimRight = 249;
constexpr float RimBottom = 194;
constexpr float RimCornerRadius = 4.7f;
constexpr float InsideLeft = 9.4f;
constexpr float InsideTop = 8.4f;
constexpr float InsideRight = 245.4f;
constexpr float InsideBottom = 190.4f;
constexpr float InsideCornerRadius = 2;
constexpr float DarkBandLeft = 11.8f;
constexpr float DarkBandTop = 25.6f;
constexpr float DarkBandBottom = 49.5f;
constexpr float DarkBandFadeStartY = 47.5f;
constexpr float DarkBandCornerRadius = 7;

Color Gray(Formats::Byte level)
{
    return {level, level, level, 255};
}

Color Shadow(Formats::Byte alpha)
{
    return {0, 0, 0, alpha};
}

Paint VerticalGradient(float startY, Color startColor, float endY, Color endColor)
{
    return LinearGradient({0, startY}, startColor, {0, endY}, endColor);
}

// The inside: light gray at the very top, then fading to white. A darker band with round ends lies over it.
constexpr float InsideFadeEndY = 80;

ColorStop AtHeight(float y, Color color)
{
    return {y / InsideFadeEndY, color};
}

Paint InsideColors()
{
    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = {0, 0};
    paint.end = {0, InsideFadeEndY};
    paint.stops = {
        AtHeight(InsideTop, Gray(222)),
        AtHeight(13.7f, Gray(222)),
        AtHeight(14.2f, Gray(214)),
        AtHeight(49.5f, Gray(206)),
        AtHeight(InsideFadeEndY, White),
    };
    return paint;
}

void FillRoundedRectangle(VectorCanvas& canvas, float left, float top, float right, float bottom, float radius, const Paint& paint)
{
    VectorPath rectangle;
    rectangle.AddRoundedRectangle(left, top, right - left, bottom - top, radius);
    canvas.Fill(rectangle, paint);
}

void FillRectangle(VectorCanvas& canvas, float left, float top, float right, float bottom, const Paint& paint)
{
    VectorPath rectangle;
    rectangle.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(rectangle, paint);
}

void DrawButtonFrame(VectorCanvas& canvas)
{
    constexpr float RightShadowStart = 236.5f;
    constexpr float BottomShadowStart = 182.5f;
    constexpr Formats::Byte RightShadowAlpha = 54;
    constexpr Formats::Byte BottomShadowAlpha = 74;
    constexpr float CaptionLineTop = 140;

    FillRoundedRectangle(canvas, FrameLeft, FrameTop, FrameRight, FrameBottom, FrameCornerRadius, SolidPaint(White));
    FillRoundedRectangle(canvas, RimLeft, RimTop, RimRight, RimBottom, RimCornerRadius, SolidPaint(RimBlue));
    FillRoundedRectangle(canvas, InsideLeft, InsideTop, InsideRight, InsideBottom, InsideCornerRadius, InsideColors());
    FillRoundedRectangle(canvas, DarkBandLeft, DarkBandTop, InsideRight, DarkBandBottom, DarkBandCornerRadius,
        VerticalGradient(DarkBandFadeStartY, Gray(189), DarkBandBottom, Gray(198)));

    FillRectangle(canvas, InsideLeft, CaptionLineTop, InsideRight, CaptionLineTop + 1, SolidPaint(Gray(239)));
    FillRectangle(canvas, InsideLeft, CaptionLineTop + 1, InsideRight, CaptionLineTop + 2, SolidPaint(Gray(231)));
    FillRectangle(canvas, InsideLeft, CaptionLineTop + 2, InsideRight, CaptionLineTop + 3, SolidPaint(Gray(206)));

    FillRectangle(canvas, RightShadowStart, InsideTop, InsideRight, InsideBottom,
        LinearGradient({RightShadowStart, 0}, Shadow(0), {InsideRight, 0}, Shadow(RightShadowAlpha)));
    FillRectangle(canvas, InsideLeft, BottomShadowStart, InsideRight, InsideBottom,
        VerticalGradient(BottomShadowStart, Shadow(0), InsideBottom, Shadow(BottomShadowAlpha)));
}

// The wrench of Wii Settings: a gray body, a dark outline drawn along the middle of its edges, a light bevel inside the upper right
// edges, and a hole in the end of the handle. The corners go around clockwise from the tip of the upper jaw.
constexpr Color WrenchBody = {198, 198, 198, 255};
constexpr Color WrenchOutline = {132, 132, 132, 255};
constexpr Color WrenchBevel = {231, 231, 231, 255};
constexpr Color WrenchHoleMiddle = {156, 156, 156, 255};
constexpr Color WrenchJawFace = {165, 165, 165, 255};
constexpr float WrenchOutlineWidth = 3.6f;
constexpr float WrenchBevelWidth = 2.4f;
constexpr Point HandleEndCenter = {168.6f, 123.2f};
constexpr float HandleEndRadius = 8.9f;
constexpr Point HoleCenter = {168, 122.8f};
constexpr float HoleRadius = 3.5f;
constexpr float HoleMiddleRadius = 0.8f;

const std::vector<Point> WrenchHeadAndHandle = {
    {92.5f, 33.5f}, {95, 31.3f}, {113, 32.3f}, {115.5f, 35}, {124.5f, 46.5f}, {125.8f, 48}, {126.2f, 67.5f}, {127, 69},
    {174.9f, 116.9f},
};

const std::vector<Point> WrenchLowerSide = {
    {162.3f, 129.5f}, {115.5f, 82.5f}, {113, 80.2f}, {94.5f, 79.8f}, {83.5f, 69.5f}, {81.3f, 60.5f}, {79.8f, 50}, {80.5f, 47},
    {83, 45.8f}, {86, 46.5f}, {87.5f, 49}, {98, 57.5f}, {104, 54}, {92.5f, 37},
};

const std::vector<Point> WrenchBevelLine = {
    {95, 34.4f}, {112.5f, 35}, {113.5f, 37}, {122.5f, 48.5f}, {123.3f, 50}, {123.3f, 67}, {124.9f, 71.1f}, {173.4f, 119.6f},
};

VectorPath WrenchShape()
{
    VectorPath wrench;
    for (Point corner : WrenchHeadAndHandle)
        wrench.LineTo(corner);
    wrench.ArcTo(HandleEndCenter, HandleEndRadius, HandleEndRadius, -45, 135);
    for (Point corner : WrenchLowerSide)
        wrench.LineTo(corner);
    wrench.Close();
    return wrench;
}

// Every piece of the line is a stroke of its own, so the round ends make round corners where two pieces meet.
void DrawLine(VectorCanvas& canvas, const std::vector<Point>& points, float width, Color color, bool isClosed)
{
    std::size_t pieceCount = isClosed ? points.size() : points.size() - 1;
    for (std::size_t index = 0; index < pieceCount; ++index)
    {
        VectorPath stroke;
        stroke.AddStroke(points[index], points[(index + 1) % points.size()], width);
        canvas.Fill(stroke, SolidPaint(color));
    }
}

void DrawWrench(VectorCanvas& canvas)
{
    VectorPath wrench = WrenchShape();
    canvas.Fill(wrench, SolidPaint(WrenchBody));

    VectorPath jawFace;
    jawFace.AddPolygon({{96.5f, 58.5f}, {106.5f, 54.5f}, {106.5f, 61.5f}, {98, 61.5f}});
    canvas.Fill(jawFace, SolidPaint(WrenchJawFace));
    DrawLine(canvas, WrenchBevelLine, WrenchBevelWidth, WrenchBevel, false);

    for (const Outline& outline : wrench.Outlines())
        DrawLine(canvas, outline, WrenchOutlineWidth, WrenchOutline, true);

    VectorPath hole;
    hole.AddCircle(HoleCenter, HoleRadius);
    canvas.Fill(hole, SolidPaint(WrenchOutline));
    VectorPath holeMiddle;
    holeMiddle.AddCircle(HoleCenter, HoleMiddleRadius);
    canvas.Fill(holeMiddle, SolidPaint(WrenchHoleMiddle));
}

}

void DrawWiiSettingsButton(VectorCanvas& canvas)
{
    DrawButtonFrame(canvas);
    DrawWrench(canvas);
}

}
