#include "SharpArt/KeyboardPictures.h"

#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color Black = {0, 0, 0, 255};
constexpr Color White = {255, 255, 255, 255};

constexpr Color Gray(Formats::Byte level)
{
    return {level, level, level, 255};
}

// The colors of a gradient from the top to the bottom, at the heights the stops give in units.
Paint VerticalGradient(const std::vector<ColorStop>& stopsByHeight)
{
    float lastHeight = stopsByHeight.back().share;

    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = {0, 0};
    paint.end = {0, lastHeight};
    for (const ColorStop& stop : stopsByHeight)
        paint.stops.push_back({stop.share / lastHeight, stop.color});
    return paint;
}

void FillRoundedBox(VectorCanvas& canvas, float left, float top, float right, float bottom, float radius, const Paint& paint)
{
    VectorPath box;
    box.AddRoundedRectangle(left, top, right - left, bottom - top, radius);
    canvas.Fill(box, paint);
}

// ---- The glass keys

constexpr float KeyEdgeWidth = 1;
constexpr float GlossInset = 2.5f;
constexpr float GlossTop = 1.3f;
constexpr float GlossBottom = 7.3f;
constexpr float GlossCornerRadius = 2.5f;

struct GlassKey
{
    float width = 0;
    float height = 0;
    float cornerRadius = 0;
    std::vector<ColorStop> glossByHeight;
    std::vector<ColorStop> bodyByHeight;
};

void DrawGlassKey(VectorCanvas& canvas, const GlassKey& key)
{
    float insideRight = key.width - KeyEdgeWidth;
    float insideBottom = key.height - KeyEdgeWidth;
    float insideRadius = key.cornerRadius;

    FillRoundedBox(canvas, 0, 0, key.width, key.height, key.cornerRadius, SolidPaint(Black));
    FillRoundedBox(canvas, KeyEdgeWidth, KeyEdgeWidth, insideRight, insideBottom, insideRadius, VerticalGradient(key.bodyByHeight));
    FillRoundedBox(canvas, GlossInset, GlossTop, key.width - GlossInset, GlossBottom, GlossCornerRadius, VerticalGradient(key.glossByHeight));
}

// The keys of one row are 34 units high and share their colors.
const std::vector<ColorStop> RowKeyGloss = {
    {1.5f, Gray(178)}, {2.5f, Gray(232)}, {3.5f, Gray(206)}, {4.5f, Gray(192)}, {5.5f, Gray(184)}, {6.5f, Gray(180)}, {GlossBottom, Gray(172)}};
const std::vector<ColorStop> RowKeyBody = {{3, Gray(135)}, {8.5f, Gray(147)}, {32.5f, Gray(222)}};
constexpr float RowKeyHeight = 34;
constexpr float RowKeyCornerRadius = 3;

}

void DrawLetterKey(VectorCanvas& canvas)
{
    constexpr float Width = 42;
    DrawGlassKey(canvas, {Width, RowKeyHeight, RowKeyCornerRadius, RowKeyGloss, RowKeyBody});
}

void DrawWideKey(VectorCanvas& canvas)
{
    constexpr float Width = 72;
    DrawGlassKey(canvas, {Width, RowKeyHeight, RowKeyCornerRadius, RowKeyGloss, RowKeyBody});
}

void DrawSpaceKey(VectorCanvas& canvas)
{
    constexpr float Width = 156;
    DrawGlassKey(canvas, {Width, RowKeyHeight, RowKeyCornerRadius, RowKeyGloss, RowKeyBody});
}

void DrawTallKey(VectorCanvas& canvas)
{
    constexpr float Width = 70;
    constexpr float Height = 66;
    constexpr float CornerRadius = 4.5f;
    const std::vector<ColorStop> gloss = {
        {1.5f, Gray(151)}, {2.5f, Gray(229)}, {3.5f, Gray(215)}, {4.5f, Gray(200)}, {5.5f, Gray(186)}, {6.5f, Gray(172)}, {GlossBottom, Gray(160)}};
    const std::vector<ColorStop> body = {{3, Gray(130)}, {8.5f, Gray(137)}, {64.5f, Gray(254)}};
    DrawGlassKey(canvas, {Width, Height, CornerRadius, gloss, body});
}

void DrawRoundPlate(VectorCanvas& canvas)
{
    constexpr float Size = 32;
    constexpr float CornerRadius = 3.5f;
    constexpr float EdgeWidth = 1;
    FillRoundedBox(canvas, 0, 0, Size, Size, CornerRadius, SolidPaint(Black));
    FillRoundedBox(canvas, EdgeWidth, EdgeWidth, Size - EdgeWidth, Size - EdgeWidth, CornerRadius - EdgeWidth, SolidPaint(White));
}

}
