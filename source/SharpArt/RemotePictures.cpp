#include "SharpArt/RemotePictures.h"

#include "SharpArt/SmallWords.h"
#include "SharpArt/WiiLogoPictures.h"

#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};
constexpr Color Black = {0, 0, 0, 255};

// The signs and words on the buttons are gray, the outlines black.
constexpr Color SignGray = {136, 136, 136, 255};

Color Gray(Formats::Byte level)
{
    return {level, level, level, 255};
}

void FillCircle(VectorCanvas& canvas, Point center, float radius, Color color)
{
    VectorPath circle;
    circle.AddCircle(center, radius);
    canvas.Fill(circle, SolidPaint(color));
}

void FillEllipse(VectorCanvas& canvas, Point center, float radiusX, float radiusY, Color color)
{
    VectorPath ellipse;
    ellipse.AddEllipse(center, radiusX, radiusY);
    canvas.Fill(ellipse, SolidPaint(color));
}

void FillRectangle(VectorCanvas& canvas, float left, float top, float right, float bottom, Color color)
{
    VectorPath rectangle;
    rectangle.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(rectangle, SolidPaint(color));
}

// A ring drawn with a pen of the width along the circle of the radius.
void DrawRing(VectorCanvas& canvas, Point center, float radius, float penWidth, Color color)
{
    VectorPath ring;
    ring.AddCircle(center, radius + penWidth / 2);
    ring.AddCircle(center, radius - penWidth / 2);
    canvas.Fill(ring, SolidPaint(color));
}

// Lines drawn one after the other with a round pen. Each line is filled on its own, so where two of them cross they do not cut a hole.
void DrawLines(VectorCanvas& canvas, const std::vector<Point>& points, float penWidth, Color color)
{
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        VectorPath line;
        line.AddStroke(points[index - 1], points[index], penWidth);
        canvas.Fill(line, SolidPaint(color));
    }
}

// ---- The body

constexpr float BodyLeft = 9.96f;
constexpr float BodyTop = 17.04f;
constexpr float BodyRight = 83.66f;
constexpr float BodyBottom = 315.54f;
constexpr float BodyCornerRadius = 13.7f;
constexpr float BodyEdgeWidth = 0.55f;
constexpr Color BodyEdgeGray = {40, 40, 40, 255};

// The body is white in the upper part and turns light gray along a slanted line, faster in the middle than at the ends.
constexpr Point BodyShadeStart = {47, 64};
constexpr Point BodyShadeDirection = {0.403f, 0.915f};
const std::vector<ColorStop> BodyShadeByDistance = {
    {0, Gray(255)},
    {7.3f, Gray(254)},
    {22, Gray(250)},
    {36.6f, Gray(244)},
    {51, Gray(238)},
    {66, Gray(231)},
    {80.5f, Gray(224)},
    {95, Gray(218)},
    {110, Gray(213)},
    {124.4f, Gray(210)},
};

Paint BodyShade()
{
    float length = BodyShadeByDistance.back().share;

    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = BodyShadeStart;
    paint.end = {BodyShadeStart.x + BodyShadeDirection.x * length, BodyShadeStart.y + BodyShadeDirection.y * length};
    for (const ColorStop& stop : BodyShadeByDistance)
        paint.stops.push_back({stop.share / length, stop.color});
    return paint;
}

// ---- The buttons

constexpr float OutlinePenWidth = 1.31f;
constexpr float SignPenWidth = 0.9f;

constexpr Point PowerButtonCenter = {27.24f, 31.06f};
constexpr float SmallButtonRadius = 5.76f;
constexpr Point AButtonCenter = {46.38f, 110.38f};
constexpr float AButtonRadius = 11.86f;
constexpr float AButtonPenWidth = 1.5f;
constexpr float SmallButtonsY = 160.06f;
constexpr float MinusButtonX = 26.24f;
constexpr float HomeButtonX = 46.24f;
constexpr float PlusButtonX = 66.66f;
constexpr float NumberButtonsX = 46.44f;
constexpr float OneButtonY = 237.14f;
constexpr float TwoButtonY = 263.14f;
constexpr float NumberButtonRadius = 7.82f;
constexpr float NumberButtonPenWidth = 1.61f;

// The cross: the outline of two crossing bars with slightly round corners, and a short gray line in each arm.
constexpr float CrossVerticalLeft = 40.6f;
constexpr float CrossVerticalRight = 52.4f;
constexpr float CrossTop = 47.8f;
constexpr float CrossBottom = 83.4f;
constexpr float CrossHorizontalTop = 59.6f;
constexpr float CrossHorizontalBottom = 71.8f;
constexpr float CrossLeft = 28.5f;
constexpr float CrossRight = 64.4f;
constexpr float CrossCornerCut = 2.2f;
constexpr float CrossMiddleX = 46.5f;
constexpr float CrossMiddleY = 65.6f;
constexpr float CrossLinePenWidth = 1.2f;

void DrawCross(VectorCanvas& canvas)
{
    std::vector<Point> corners = {
        {CrossVerticalLeft, CrossTop},
        {CrossVerticalRight, CrossTop},
        {CrossVerticalRight, CrossHorizontalTop},
        {CrossRight, CrossHorizontalTop},
        {CrossRight, CrossHorizontalBottom},
        {CrossVerticalRight, CrossHorizontalBottom},
        {CrossVerticalRight, CrossBottom},
        {CrossVerticalLeft, CrossBottom},
        {CrossVerticalLeft, CrossHorizontalBottom},
        {CrossLeft, CrossHorizontalBottom},
        {CrossLeft, CrossHorizontalTop},
        {CrossVerticalLeft, CrossHorizontalTop},
    };
    float halfPen = OutlinePenWidth / 2;
    std::vector<Point> outside;
    std::vector<Point> inside;
    for (Point corner : corners)
    {
        float outwardX = corner.x <= CrossMiddleX ? -halfPen : halfPen;
        float outwardY = corner.y <= CrossMiddleY ? -halfPen : halfPen;
        outside.push_back({corner.x + outwardX, corner.y + outwardY});
        inside.push_back({corner.x - outwardX, corner.y - outwardY});
    }

    VectorPath outline;
    outline.AddPolygonWithRoundCorners(outside, CrossCornerCut);
    outline.AddPolygonWithRoundCorners(inside, CrossCornerCut);
    canvas.Fill(outline, SolidPaint(Black));

    DrawLines(canvas, {{CrossMiddleX, 52}, {CrossMiddleX, 59}}, CrossLinePenWidth, SignGray);
    DrawLines(canvas, {{CrossMiddleX, 73.5f}, {CrossMiddleX, 80.5f}}, CrossLinePenWidth, SignGray);
    DrawLines(canvas, {{33, CrossMiddleY + 0.4f}, {39.5f, CrossMiddleY + 0.4f}}, CrossLinePenWidth, SignGray);
    DrawLines(canvas, {{53.5f, CrossMiddleY + 0.4f}, {60, CrossMiddleY + 0.4f}}, CrossLinePenWidth, SignGray);
}

void DrawLetterA(VectorCanvas& canvas)
{
    DrawLines(canvas, {{43.6f, 112.8f}, {46.2f, 107.3f}, {48.6f, 112.8f}}, 1.1f, SignGray);
    DrawLines(canvas, {{44.6f, 111.3f}, {47.7f, 111.3f}}, 0.9f, SignGray);
}

void DrawMinusAndPlusSigns(VectorCanvas& canvas)
{
    FillRectangle(canvas, 23, 159, 28, 161, SignGray);
    FillRectangle(canvas, 64, 159, 69.6f, 161, SignGray);
    FillRectangle(canvas, 66, 157, 67.6f, 163, SignGray);
}

void DrawNumberOne(VectorCanvas& canvas)
{
    DrawLines(canvas, {{44.4f, 235.4f}, {46.4f, 234.4f}, {46.4f, 239.3f}}, 1.3f, SignGray);
}

void DrawNumberTwo(VectorCanvas& canvas)
{
    VectorPath top;
    top.ArcTo({46.1f, 262.3f}, 1.6f, 1.5f, 200, 360);
    std::vector<Point> curve = top.Outlines().front();
    curve.push_back({44.4f, 265.5f});
    curve.push_back({47.6f, 265.5f});
    DrawLines(canvas, curve, 1.1f, SignGray);
}

// ---- The small words

constexpr float WordHeight = 2.8f;
constexpr float WordLetterWidth = 2.1f;
constexpr float WordLetterPitch = 2.95f;
constexpr float WordPenWidth = 1;

void DrawWord(VectorCanvas& canvas, std::string_view word, Point topLeft)
{
    DrawSmallWord(canvas, word, {topLeft, WordLetterWidth, WordHeight, WordLetterPitch, WordPenWidth, SignGray});
}

// ---- The speaker and the lights

constexpr float SpeakerColumns[] = {41.5f, 46.5f, 51.5f};
constexpr float SpeakerRows[] = {184.5f, 189.7f, 195, 200.5f, 205.9f, 211.4f};
constexpr float SmallHoleRadius = 1.06f;
constexpr Color HoleColor = {0, 0, 0, 170};
constexpr float LargeHoleWidth = 2.6f;
constexpr float LargeHoleHeight = 3.1f;
constexpr float LargeHoleCornerRadius = 0.9f;

void DrawSpeakerHoles(VectorCanvas& canvas)
{
    constexpr std::size_t FirstRow = 0;
    constexpr std::size_t LastRow = 5;
    for (std::size_t row = FirstRow; row <= LastRow; ++row)
    {
        bool isEndRow = row == FirstRow || row == LastRow;
        for (float x : SpeakerColumns)
        {
            if (isEndRow)
            {
                FillCircle(canvas, {x, SpeakerRows[row]}, SmallHoleRadius, HoleColor);
                continue;
            }
            VectorPath hole;
            hole.AddRoundedRectangle(x - LargeHoleWidth / 2, SpeakerRows[row] - LargeHoleHeight / 2, LargeHoleWidth, LargeHoleHeight,
                LargeHoleCornerRadius);
            canvas.Fill(hole, SolidPaint(HoleColor));
        }
    }
}

// Above each light of a player as many dots as its number, and the light below them, a faint square.
constexpr float PlayerDotsY = 279.5f;
constexpr float PlayerDotRadius = 0.93f;
constexpr float PlayerDotSpacing = 2.4f;
constexpr float PlayerLightsLeft[] = {25, 38, 51, 64};
constexpr float PlayerLightSize = 4;
constexpr float PlayerLightTop = 282;
constexpr Color PlayerDotColor = {0, 0, 0, 110};
constexpr Color PlayerLightColor = {0, 0, 0, 108};

void DrawPlayerLights(VectorCanvas& canvas)
{
    for (std::size_t player = 0; player < std::size(PlayerLightsLeft); ++player)
    {
        float lightLeft = PlayerLightsLeft[player];
        float lightMiddle = lightLeft + PlayerLightSize / 2;
        std::size_t dotCount = player + 1;
        float dotsWidth = PlayerDotSpacing * static_cast<float>(dotCount - 1);
        for (std::size_t dot = 0; dot < dotCount; ++dot)
        {
            float x = lightMiddle - dotsWidth / 2 + PlayerDotSpacing * static_cast<float>(dot);
            FillCircle(canvas, {x, PlayerDotsY}, PlayerDotRadius, PlayerDotColor);
        }
        FillRectangle(canvas, lightLeft, PlayerLightTop, lightLeft + PlayerLightSize, PlayerLightTop + PlayerLightSize, PlayerLightColor);
    }
}

constexpr Point WiiWordOffset = {32.1f, 292.4f};
constexpr float WiiWordScale = 0.4015f;

void DrawWiiWord(VectorCanvas& canvas)
{
    VectorPath word;
    AddWiiWord(word, WiiWordOffset, WiiWordScale);
    canvas.Fill(word, SolidPaint(SignGray));
}

}

void DrawRemoteBody(VectorCanvas& canvas)
{
    VectorPath edge;
    edge.AddRoundedRectangle(BodyLeft, BodyTop, BodyRight - BodyLeft, BodyBottom - BodyTop, BodyCornerRadius);
    canvas.Fill(edge, SolidPaint(BodyEdgeGray));

    float inset = BodyEdgeWidth;
    VectorPath body;
    body.AddRoundedRectangle(BodyLeft + inset, BodyTop + inset, BodyRight - BodyLeft - 2 * inset, BodyBottom - BodyTop - 2 * inset,
        BodyCornerRadius - inset);
    canvas.Fill(body, BodyShade());
}

void DrawRemoteButtons(VectorCanvas& canvas)
{
    DrawRing(canvas, PowerButtonCenter, SmallButtonRadius, OutlinePenWidth, Black);
    DrawWord(canvas, "POWER", {20.2f, 39.9f});
    DrawCross(canvas);
    DrawRing(canvas, AButtonCenter, AButtonRadius, AButtonPenWidth, Black);
    DrawLetterA(canvas);

    DrawRing(canvas, {MinusButtonX, SmallButtonsY}, SmallButtonRadius, OutlinePenWidth, Black);
    DrawRing(canvas, {HomeButtonX, SmallButtonsY}, SmallButtonRadius, OutlinePenWidth, Black);
    DrawRing(canvas, {PlusButtonX, SmallButtonsY}, SmallButtonRadius, OutlinePenWidth, Black);
    DrawMinusAndPlusSigns(canvas);
    DrawWord(canvas, "HOME", {40.3f, 169.7f});

    DrawSpeakerHoles(canvas);
    DrawRing(canvas, {NumberButtonsX, OneButtonY}, NumberButtonRadius, NumberButtonPenWidth, Black);
    DrawRing(canvas, {NumberButtonsX, TwoButtonY}, NumberButtonRadius, NumberButtonPenWidth, Black);
    DrawNumberOne(canvas);
    DrawNumberTwo(canvas);
    DrawPlayerLights(canvas);
    DrawWiiWord(canvas);
}

void DrawRemoteHomeSign(VectorCanvas& canvas)
{
    VectorPath house;
    house.AddPolygon({{7.5f, 3.9f}, {11.9f, 8.5f}, {10.1f, 8.5f}, {10.1f, 12}, {4.4f, 12}, {4.4f, 8.5f}, {3.1f, 8.5f}});
    house.AddRectangle(6.3f, 9.2f, 2, 1.6f);
    canvas.Fill(house, SolidPaint(White));
}

void DrawRemotePowerSign(VectorCanvas& canvas)
{
    constexpr Point Center = {7.5f, 8.2f};
    constexpr float Radius = 2.6f;
    constexpr float PenWidth = 1.2f;

    VectorPath arc;
    arc.ArcTo(Center, Radius, Radius, -55, 235);
    DrawLines(canvas, arc.Outlines().front(), PenWidth, White);
    DrawLines(canvas, {{Center.x, 4.4f}, {Center.x, 8.6f}}, PenWidth, White);
}

void DrawRemoteSpeakerLit(VectorCanvas& canvas)
{
    constexpr float Columns[] = {3.7f, 8.45f, 13.25f};
    struct LitRow
    {
        float y;
        float radiusX;
        float radiusY;
    };
    constexpr LitRow Rows[] = {{3.16f, 1.6f, 1.97f}, {7.93f, 1.8f, 2.1f}, {13.7f, 2.35f, 2.35f}, {19.28f, 2.35f, 2.35f}, {25, 1.8f, 2.1f}, {29.57f, 1.75f, 1.59f}};
    for (const LitRow& row : Rows)
    {
        for (float x : Columns)
            FillEllipse(canvas, {x, row.y}, row.radiusX, row.radiusY, White);
    }
}

}
