#include "SharpArt/KeyboardPictures.h"

#include <array>
#include <initializer_list>
#include <utility>
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

// ---- The signs on the keys and the buttons of the keyboard

namespace
{

constexpr Color IconDark = {17, 17, 17, 255};
constexpr Color ArrowDark = {34, 34, 34, 255};

void FillPolygon(VectorCanvas& canvas, std::initializer_list<Point> corners, Color color)
{
    VectorPath polygon;
    polygon.AddPolygon(corners);
    canvas.Fill(polygon, SolidPaint(color));
}

void FillBox(VectorCanvas& canvas, float left, float top, float right, float bottom, Color color)
{
    VectorPath box;
    box.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(box, SolidPaint(color));
}

// A row of white keys of the keyboard sign: their tops, bottoms and the left and right sides of each key.
struct KeyRow
{
    float top = 0;
    float bottom = 0;
    std::vector<std::pair<float, float>> keys;
};

}

void DrawBackspaceSign(VectorCanvas& canvas)
{
    FillPolygon(canvas, {{0.3f, 16}, {22, 2.2f}, {22, 12.3f}, {56, 12.3f}, {56, 20.5f}, {22, 20.5f}, {22, 29.8f}}, White);
}

void DrawEnterSign(VectorCanvas& canvas)
{
    VectorPath sign;
    sign.MoveTo({3.4f, 32});
    sign.LineTo({24.8f, 18.2f});
    sign.LineTo({24.8f, 28.3f});
    sign.LineTo({38.5f, 28.3f});
    sign.LineTo({41.5f, 27.6f});
    sign.LineTo({43.6f, 25.5f});
    sign.LineTo({45.1f, 23.5f});
    sign.LineTo({46, 20.5f});
    sign.LineTo({46, 2});
    sign.LineTo({53, 2});
    sign.LineTo({53, 21});
    sign.CurveTo({53, 28}, {46, 35.2f}, {38, 35.2f});
    sign.LineTo({24.8f, 35.2f});
    sign.LineTo({24.8f, 45.8f});
    sign.Close();
    canvas.Fill(sign, SolidPaint(White));
}

void DrawShiftSign(VectorCanvas& canvas)
{
    FillPolygon(canvas, {{27.5f, 1.8f}, {52.8f, 19.7f}, {38.3f, 19.7f}, {38.3f, 29}, {16.7f, 29}, {16.7f, 19.7f}, {2.6f, 19.7f}}, IconDark);
    FillPolygon(canvas, {{27.5f, 4.6f}, {46.5f, 17.6f}, {36.3f, 17.6f}, {36.3f, 27}, {18.7f, 27}, {18.7f, 17.6f}, {8.5f, 17.6f}}, White);
}

void DrawFullKeyboardSign(VectorCanvas& canvas)
{
    constexpr float BodyCornerRadius = 2;
    FillRoundedBox(canvas, 3, 3, 61, 29, BodyCornerRadius, SolidPaint(IconDark));

    const std::vector<KeyRow> rows = {
        {5, 9, {{5, 10}, {12.2f, 17}, {19, 24}, {26, 31}, {33, 38}, {40, 45}, {47, 52}, {54, 59}}},
        {11, 15, {{5, 13}, {15.2f, 20}, {22.2f, 27}, {29.2f, 34}, {36.2f, 41.5f}, {43.5f, 48.5f}, {51, 59}}},
        {17, 21, {{5, 16}, {18, 23}, {25, 30}, {32, 37}, {39, 44}, {46, 50}, {52, 59}}},
        {23, 27, {{5, 10}, {12, 17}, {19.5f, 37}, {40.5f, 45}, {47.2f, 52}, {54.2f, 59}}},
    };
    for (const KeyRow& row : rows)
    {
        for (const auto& [left, right] : row.keys)
            FillBox(canvas, left, row.top, right, row.bottom, White);
    }
}

void DrawTenKeySign(VectorCanvas& canvas)
{
    constexpr float BodyCornerRadius = 1.5f;
    constexpr std::array<float, 3> ColumnLefts = {12, 26, 40};
    constexpr float KeyWidth = 12;
    constexpr std::array<float, 4> RowTops = {3.3f, 10, 17, 24};
    constexpr std::array<float, 4> RowBottoms = {8, 15, 22, 29};

    FillRoundedBox(canvas, 10, 1, 54, 31, BodyCornerRadius, SolidPaint(IconDark));
    for (std::size_t row = 0; row < RowTops.size(); ++row)
    {
        for (float left : ColumnLefts)
            FillBox(canvas, left, RowTops[row], left + KeyWidth, RowBottoms[row], White);
    }
}

void DrawScrollArrowSign(VectorCanvas& canvas)
{
    constexpr float Size = 48;
    constexpr float CornerRadius = 7;
    constexpr float EdgeWidth = 3;
    FillRoundedBox(canvas, 0, 0, Size, Size, CornerRadius, SolidPaint(IconDark));
    FillRoundedBox(canvas, EdgeWidth, EdgeWidth, Size - EdgeWidth, Size - EdgeWidth, CornerRadius - EdgeWidth, SolidPaint(White));
    FillPolygon(canvas, {{24, 8.8f}, {40.8f, 37.8f}, {24, 31.2f}, {7.2f, 37.8f}}, ArrowDark);
}

}
