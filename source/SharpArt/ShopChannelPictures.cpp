#include "SharpArt/ShopChannelPictures.h"

#include "SharpArt/CardPerspective.h"
#include "SharpArt/WiiLogoPictures.h"

#include <array>
#include <cmath>
#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};
constexpr Color HandleBlue = {0, 187, 255, 255};
constexpr Color SmallHandleBlue = {0, 119, 255, 255};
constexpr Color LightCardBlue = {0, 187, 255, 255};
constexpr Color DarkCardBlue = {0, 119, 255, 255};

// ---- The cards of the Shop with the word Wii on them, seen in perspective

using CardCorners = std::array<Point, 4>;

// The word as the Wii logo has it (WiiLogoPictures), from the left of the W to the right of the second i and from the top of the W to
// its bottom.
constexpr float WordLeft = 6.2f;
constexpr float WordTop = 5;
constexpr float WordRight = 62.13f;
constexpr float WordBottom = 30.4f;

// Where the word lies on every card, in the square from (0, 0) to (1, 1) of the card.
constexpr float WordOnCardLeft = 0.3f;
constexpr float WordOnCardTop = 0.362f;
constexpr float WordOnCardRight = 0.71f;
constexpr float WordOnCardBottom = 0.605f;

Point PlaceOnCard(Point pointOfTheWord)
{
    float acrossShare = (pointOfTheWord.x - WordLeft) / (WordRight - WordLeft);
    float downShare = (pointOfTheWord.y - WordTop) / (WordBottom - WordTop);
    return {WordOnCardLeft + acrossShare * (WordOnCardRight - WordOnCardLeft), WordOnCardTop + downShare * (WordOnCardBottom - WordOnCardTop)};
}

VectorPath WiiWordOnCard(const CardCorners& corners)
{
    VectorPath word;
    AddWiiWord(word, {0, 0}, 1);

    CardPerspective perspective(corners);
    word.PlaceEveryPoint([&perspective](Point pointOfTheWord) { return perspective.Place(PlaceOnCard(pointOfTheWord)); });
    return word;
}

// How far each corner of a card is cut to make it round, in the order of the corners.
using CornerCuts = std::array<float, 4>;

VectorPath CardShape(const CardCorners& corners, const CornerCuts& cuts)
{
    std::vector<RoundCorner> roundCorners;
    for (std::size_t index = 0; index < corners.size(); ++index)
        roundCorners.push_back({corners[index], cuts[index]});

    VectorPath card;
    card.AddPolygonWithRoundCorners(roundCorners);
    return card;
}

// The big card of the banner's logo: a white edge around a blue card with the word in white. Its sides bulge a little, so its outlines
// are measured all around; the word lies on the card's four corners.
const std::vector<Point> LogoCardEdgeOutline = {{2.6f, 3}, {6, 1.7f}, {16.5f, 2.1f}, {28.5f, 4.4f}, {40.5f, 6.6f}, {52.5f, 9}, {64.5f, 11.8f},
    {76.5f, 14.1f}, {88.5f, 17}, {100.5f, 19.9f}, {112.5f, 23}, {124.5f, 27.9f}, {128.6f, 34.5f}, {129.8f, 46.5f}, {129.9f, 58.5f},
    {129.6f, 70.5f}, {129.6f, 82.5f}, {129.1f, 94.5f}, {128.2f, 106.5f}, {126.9f, 118.5f}, {125.6f, 122.5f}, {124, 125.2f}, {120, 126.4f}, {115.5f, 125.8f},
    {103.5f, 123}, {91.5f, 120}, {79.5f, 117.1f}, {67.5f, 113.9f}, {55.5f, 110.6f}, {43.5f, 107.1f}, {31.5f, 103.6f}, {19.5f, 99.6f},
    {13, 97.5f}, {7.5f, 92.9f}, {4.9f, 85.5f}, {3.6f, 73.5f}, {2.6f, 61.5f}, {1.9f, 49.5f}, {1.2f, 37.5f}, {1, 25.5f}, {1.1f, 13.5f}, {1.4f, 6}};
const std::vector<Point> LogoCardOutline = {{6.6f, 6.6f}, {9, 6}, {16.5f, 6.8f}, {28.5f, 8.8f}, {40.5f, 11.1f}, {52.5f, 13.8f}, {64.5f, 16.2f}, {76.5f, 18.9f},
    {88.5f, 21.8f}, {100.5f, 24.7f}, {112.5f, 28}, {121, 31}, {123.9f, 34.5f}, {125.2f, 46.5f}, {125.3f, 58.5f}, {125.3f, 70.5f}, {125.1f, 82.5f},
    {124.8f, 94.5f}, {123.8f, 106.5f}, {122.6f, 117.5f}, {121.2f, 120.8f}, {115.5f, 121.6f}, {103.5f, 118.9f}, {91.5f, 115.9f}, {79.5f, 112.9f},
    {67.5f, 109.9f}, {55.5f, 106.6f}, {43.5f, 103}, {31.5f, 99.4f}, {19.5f, 95.8f}, {11, 92}, {8.4f, 85.5f}, {7.2f, 73.5f},
    {6.3f, 61.5f}, {5.7f, 49.5f}, {5, 37.5f}, {5, 25.5f}, {5.2f, 13.5f}, {5.3f, 9.5f}};
constexpr CardCorners LogoCard = {Point{1.2f, 2.4f}, Point{124.6f, 29.7f}, Point{125.9f, 125.3f}, Point{9.9f, 94}};
constexpr float LogoOutlineCornerCut = 4;

void FillOutline(VectorCanvas& canvas, const std::vector<Point>& outline, Color color)
{
    VectorPath shape;
    shape.AddPolygonWithRoundCorners(outline, LogoOutlineCornerCut);
    canvas.Fill(shape, SolidPaint(color));
}

void DrawLogoCard(VectorCanvas& canvas, Color cardColor)
{
    FillOutline(canvas, LogoCardEdgeOutline, White);
    FillOutline(canvas, LogoCardOutline, cardColor);
    canvas.Fill(WiiWordOnCard(LogoCard), SolidPaint(White));
}

// ---- The handles

// One side of an arch: its leg, straight from the bottom up to where it bends into the top, and how full the bend is. The bend is a
// curve to the peak inside the box of the two; its control points lie this share of the way towards the box's corner, upwards from
// the leg and sideways from the peak (0.55 makes a quarter circle).
struct ArchSide
{
    float bottomX = 0;
    Point bend;
    float upwardFullness = 0;
    float sidewaysFullness = 0;
};

// One outline of an arch, the outer or the inner one: up the left leg, round over the peak and down the right leg.
struct ArchOutline
{
    ArchSide left;
    Point peak;
    ArchSide right;
};

struct Arch
{
    float bottom = 0;
    ArchOutline outer;
    ArchOutline inner;

    // The legs end in small slanted corners: so far up and so far in from their outer and inner sides.
    float bottomCornerCut = 0;
};

void AddBendUp(VectorPath& path, const ArchSide& side, Point peak)
{
    Point bend = side.bend;
    Point upward = {bend.x, bend.y - (bend.y - peak.y) * side.upwardFullness};
    Point sideways = {peak.x - (peak.x - bend.x) * side.sidewaysFullness, peak.y};
    path.LineTo(bend);
    path.CurveTo(upward, sideways, peak);
}

void AddBendDown(VectorPath& path, const ArchSide& side, Point peak)
{
    Point bend = side.bend;
    Point upward = {bend.x, bend.y - (bend.y - peak.y) * side.upwardFullness};
    Point sideways = {peak.x - (peak.x - bend.x) * side.sidewaysFullness, peak.y};
    path.CurveTo(sideways, upward, bend);
}

// The cut moves the very bottom of the outline inwards when it is positive (the outer outline) and outwards when it is negative (the
// inner outline, the hole between the legs).
VectorPath ArchShape(const ArchOutline& outline, float bottom, float bottomCornerCut)
{
    float cornerHeight = std::abs(bottomCornerCut);

    VectorPath shape;
    shape.MoveTo({outline.left.bottomX + bottomCornerCut, bottom});
    shape.LineTo({outline.left.bottomX, bottom - cornerHeight});
    AddBendUp(shape, outline.left, outline.peak);
    AddBendDown(shape, outline.right, outline.peak);
    shape.LineTo({outline.right.bottomX, bottom - cornerHeight});
    shape.LineTo({outline.right.bottomX - bottomCornerCut, bottom});
    shape.Close();
    return shape;
}

void DrawArch(VectorCanvas& canvas, const Arch& arch, Color color)
{
    canvas.Fill(ArchShape(arch.outer, arch.bottom, arch.bottomCornerCut), SolidPaint(color));
    canvas.Cut(ArchShape(arch.inner, arch.bottom, -arch.bottomCornerCut));
}

constexpr Arch Handle = {
    .bottom = 134,
    .outer = {.left = {2.7f, {1, 47.5f}, 0.4f, 0.75f}, .peak = {45.4f, 1}, .right = {103, {103, 68.5f}, 0.55f, 0.6f}},
    .inner = {.left = {22.3f, {20.3f, 49.5f}, 0.85f, 0.3f}, .peak = {45.4f, 20}, .right = {82.7f, {82.4f, 67.5f}, 0.8f, 0.3f}},
};

constexpr Arch SmallHandle = {
    .bottom = 92,
    .outer = {.left = {0.4f, {0.4f, 44.5f}, 0.45f, 0.7f}, .peak = {38.8f, 0.4f}, .right = {87.6f, {87.6f, 54.5f}, 0.6f, 0.55f}},
    .inner = {.left = {17, {17, 46.5f}, 0.75f, 0.45f}, .peak = {39.4f, 17}, .right = {71, {71, 55.5f}, 0.75f, 0.4f}},
};

constexpr Arch HandleEdge = {
    .bottom = 144.6f,
    .outer = {.left = {2.9f, {1, 52.5f}, 0.3f, 0.85f}, .peak = {51.4f, 1}, .right = {115, {115, 69.5f}, 0.35f, 0.75f}},
    .inner = {.left = {34.1f, {32.6f, 56.5f}, 0.8f, 0.3f}, .peak = {51.4f, 32}, .right = {82.7f, {82.4f, 75.5f}, 0.75f, 0.3f}},
    .bottomCornerCut = 1.2f,
};

constexpr Arch SmallHandleEdge = {
    .bottom = 99.7f,
    .outer = {.left = {0.3f, {0.3f, 46.5f}, 0.35f, 0.75f}, .peak = {44.6f, 0.3f}, .right = {95.7f, {95.7f, 53.5f}, 0.3f, 0.8f}},
    .inner = {.left = {24.7f, {24.7f, 53.5f}, 0.85f, 0.3f}, .peak = {45.4f, 25}, .right = {71.3f, {71.3f, 59.5f}, 0.8f, 0.3f}},
    .bottomCornerCut = 1.5f,
};

}

void DrawShopLogoCard(VectorCanvas& canvas)
{
    DrawLogoCard(canvas, LightCardBlue);
}

void DrawShopDarkLogoCard(VectorCanvas& canvas)
{
    DrawLogoCard(canvas, DarkCardBlue);
}

void DrawShopHandle(VectorCanvas& canvas)
{
    DrawArch(canvas, Handle, HandleBlue);
}

void DrawShopSmallHandle(VectorCanvas& canvas)
{
    DrawArch(canvas, SmallHandle, SmallHandleBlue);
}

void DrawShopHandleEdge(VectorCanvas& canvas)
{
    DrawArch(canvas, HandleEdge, White);
}

void DrawShopSmallHandleEdge(VectorCanvas& canvas)
{
    DrawArch(canvas, SmallHandleEdge, White);
}

}
