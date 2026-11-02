#include "SharpArt/ShopChannelPictures.h"

#include "SharpArt/CardPerspective.h"
#include "SharpArt/WiiLogoPictures.h"

#include <array>
#include <cmath>
#include <optional>
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

// Where the word lies on a card, in the square from (0, 0) to (1, 1) of the card. The word of the backgrounds differs a little from the
// logo of the settings: its two i stand closer to the W and their dots lie lower, by these lengths in the units of the logo.
struct WordPlace
{
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
    float closerLetters = 0;
    float lowerDots = 0;
};

constexpr WordPlace OnTheLogoCard = {0.3f, 0.362f, 0.71f, 0.605f};
constexpr WordPlace OnTheBackgroundCards = {0.334f, 0.399f, 0.668f, 0.605f, 1, 1};
constexpr WordPlace OnTheSmallBackgroundCard = {0.326f, 0.399f, 0.664f, 0.603f};

// The W ends here and the i begin, and their dots end here, in the units of the logo.
constexpr float EndOfTheW = 44;
constexpr float BottomOfTheDots = 10;

Point PlaceOnCard(Point pointOfTheWord, const WordPlace& place)
{
    bool isOnAnI = pointOfTheWord.x > EndOfTheW;
    if (isOnAnI)
        pointOfTheWord.x -= place.closerLetters;
    if (isOnAnI && pointOfTheWord.y < BottomOfTheDots)
        pointOfTheWord.y += place.lowerDots;

    float acrossShare = (pointOfTheWord.x - WordLeft) / (WordRight - WordLeft);
    float downShare = (pointOfTheWord.y - WordTop) / (WordBottom - WordTop);
    return {place.left + acrossShare * (place.right - place.left), place.top + downShare * (place.bottom - place.top)};
}

VectorPath WiiWordOnCard(const CardCorners& corners, const WordPlace& place)
{
    VectorPath word;
    AddWiiWord(word, {0, 0}, 1);

    CardPerspective perspective(corners);
    word.PlaceEveryPoint([&perspective, &place](Point pointOfTheWord) { return perspective.Place(PlaceOnCard(pointOfTheWord, place)); });
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
constexpr float OutlineCornerCut = 4;

void FillOutline(VectorCanvas& canvas, const std::vector<Point>& outline, Color color)
{
    VectorPath shape;
    shape.AddPolygonWithRoundCorners(outline, OutlineCornerCut);
    canvas.Fill(shape, SolidPaint(color));
}

void DrawLogoCard(VectorCanvas& canvas, Color cardColor)
{
    FillOutline(canvas, LogoCardEdgeOutline, White);
    FillOutline(canvas, LogoCardOutline, cardColor);
    canvas.Fill(WiiWordOnCard(LogoCard, OnTheLogoCard), SolidPaint(White));
}

// ---- The white backgrounds with windows in the shape of cards, through which the colors behind them show, and the word Wii in each

// The corners of a card are round by these shares of the length of its top side, in the order of the corners. Of the card cut off at the
// bottom of the big background only the upper part shows: its lower corners are measured on its visible lower left corner, and its word
// lies on a card that goes on further down. The word of the card cut off at the right lies on a card a little higher.
using CornerRoundness = std::array<float, 4>;
constexpr CornerRoundness EvenlyRound = {0.09f, 0.09f, 0.09f, 0.09f};
constexpr CornerRoundness ClippedCardRoundness = {0.03f, 0.08f, 0.08f, 0.06f};
constexpr CornerRoundness RightClippedCardRoundness = {0.07f, 0.09f, 0.09f, 0.06f};

struct BackgroundCard
{
    CardCorners corners;
    CornerRoundness roundness = EvenlyRound;

    // The card the word lies on, when it is not the window itself.
    std::optional<CardCorners> wordCard;
};

// The big background (943 by 565): rows of cards that grow towards the right, as if on a curved wall.
const std::vector<BackgroundCard> BackgroundCards = {
    {{Point{259.2f, -0.9f}, Point{355.6f, 5.1f}, Point{357.9f, 93.1f}, Point{266.2f, 84.3f}}},
    {{Point{512.5f, 13.6f}, Point{633.6f, 20.7f}, Point{620.3f, 118.3f}, Point{505.7f, 107.7f}}},
    {{Point{663.3f, 22.3f}, Point{800.8f, 30.1f}, Point{775.2f, 133.6f}, Point{647.2f, 121.3f}}},
    {{Point{835, 32.4f}, Point{995.9f, 41}, Point{965.3f, 152}, Point{807.6f, 135.8f}}, RightClippedCardRoundness,
        CardCorners{Point{837.8f, 37.2f}, Point{994.4f, 36.1f}, Point{957.8f, 155.4f}, Point{805.3f, 140.9f}}},
    {{Point{75, 87.8f}, Point{149.1f, 96}, Point{160.2f, 166.9f}, Point{88.7f, 157}}},
    {{Point{166.8f, 97.8f}, Point{248.4f, 106.7f}, Point{255.4f, 180.8f}, Point{177, 169.5f}}},
    {{Point{267.9f, 108.7f}, Point{357.9f, 118.6f}, Point{360.1f, 195.8f}, Point{273.8f, 183.3f}}},
    {{Point{379.5f, 120.9f}, Point{479.6f, 131.8f}, Point{475.5f, 212.2f}, Point{380.3f, 198.7f}}},
    {{Point{503.6f, 134.4f}, Point{615.4f, 146.5f}, Point{603.9f, 230.6f}, Point{498, 215.6f}}},
    {{Point{642.6f, 149.3f}, Point{768.3f, 163.1f}, Point{747.1f, 251.1f}, Point{628.9f, 234.4f}}},
    {{Point{92.6f, 176.9f}, Point{163.1f, 187.8f}, Point{173, 251.1f}, Point{104.8f, 238.7f}}},
    {{Point{179.8f, 190.4f}, Point{257, 202.2f}, Point{263.2f, 267.7f}, Point{188.9f, 254.2f}}},
    {{Point{798.6f, 166.3f}, Point{941.1f, 181.8f}, Point{907.4f, 274.2f}, Point{775.3f, 255.3f}}, {0.09f, 0.09f, 0.09f, 0.06f}},
    {{Point{275.4f, 205.1f}, Point{360, 217.9f}, Point{361.9f, 285.9f}, Point{280.7f, 271.1f}}},
    {{Point{380.4f, 221.1f}, Point{473.6f, 235.3f}, Point{470.2f, 305.9f}, Point{380.9f, 289.5f}}},
    {{Point{31.5f, 241.9f}, Point{93.6f, 253.9f}, Point{105.2f, 309.1f}, Point{45.2f, 295.9f}}},
    {{Point{108.2f, 256.6f}, Point{175.5f, 269.7f}, Point{184.4f, 326.6f}, Point{119.3f, 312.2f}}},
    {{Point{191.5f, 272.7f}, Point{264.7f, 286.9f}, Point{270, 345.4f}, Point{199.5f, 329.8f}}},
    {{Point{496.2f, 238.8f}, Point{599.8f, 254.8f}, Point{589.8f, 327.7f}, Point{491.2f, 309.9f}}},
    {{Point{624.8f, 258.5f}, Point{740.3f, 276.3f}, Point{722.2f, 352.1f}, Point{612.9f, 332.3f}}},
    {{Point{48.9f, 311.8f}, Point{108.3f, 325.2f}, Point{118.9f, 375.2f}, Point{61.2f, 360.8f}}},
    {{Point{281.9f, 290.2f}, Point{361.7f, 305.5f}, Point{363.7f, 366}, Point{286.8f, 349.1f}}},
    {{Point{380.8f, 309.2f}, Point{468.4f, 326.1f}, Point{465.6f, 388.3f}, Point{381.7f, 370}}},
    {{Point{768.3f, 280.4f}, Point{897.8f, 300.5f}, Point{870.2f, 379.3f}, Point{748, 357.1f}}},
    {{Point{122.4f, 328.5f}, Point{186.5f, 343.1f}, Point{194.6f, 394.2f}, Point{132.4f, 378.8f}}},
    {{Point{201.8f, 346.6f}, Point{271.3f, 362.3f}, Point{276.5f, 414.9f}, Point{209.2f, 398.1f}}},
    {{Point{489.5f, 330.1f}, Point{586.2f, 348.9f}, Point{577.3f, 412.9f}, Point{485.3f, 392.8f}}},
    {{Point{609.4f, 353.2f}, Point{716.3f, 374}, Point{700.6f, 440.1f}, Point{599, 417.7f}}},
    {{Point{-1.1f, 358.1f}, Point{52, 371.8f}, Point{63.7f, 416.4f}, Point{11.8f, 401.7f}}},
    {{Point{64.5f, 375.2f}, Point{121.6f, 389.8f}, Point{131.4f, 435.4f}, Point{75.8f, 419.8f}}},
    {{Point{135.1f, 393.4f}, Point{196.6f, 409.4f}, Point{203.9f, 455.7f}, Point{144.3f, 439.1f}}},
    {{Point{287.8f, 366.1f}, Point{363.7f, 383.5f}, Point{364.9f, 437.2f}, Point{292.2f, 419}}},
    {{Point{381.6f, 387.5f}, Point{464, 406.3f}, Point{461.3f, 461.5f}, Point{382, 441.6f}}},
    {{Point{742, 378.8f}, Point{861.1f, 401.9f}, Point{837, 470}, Point{725.6f, 446}}, {0.09f, 0.09f, 0.09f, 0.06f},
        CardCorners{Point{742, 378.8f}, Point{861.1f, 401.9f}, Point{837, 470}, Point{724.5f, 445.4f}}},
    {{Point{15.4f, 414.7f}, Point{66.8f, 429.5f}, Point{77.5f, 470.2f}, Point{27.5f, 455}}},
    {{Point{78.8f, 432.8f}, Point{133.9f, 448.8f}, Point{142.5f, 490.1f}, Point{89.2f, 473.9f}}},
    {{Point{211.1f, 413.1f}, Point{277.3f, 430.2f}, Point{282.2f, 477.7f}, Point{217.8f, 459.7f}}},
    {{Point{293.1f, 434.4f}, Point{364.8f, 452.8f}, Point{366.6f, 501.4f}, Point{297.2f, 482}}},
    {{Point{483.7f, 410.8f}, Point{574.3f, 431.5f}, Point{566.4f, 488.1f}, Point{479.8f, 466.2f}}},
    {{Point{30.8f, 466.6f}, Point{80.4f, 482.4f}, Point{90.3f, 519.8f}, Point{41.7f, 503.8f}}},
    {{Point{146.7f, 452.4f}, Point{205.7f, 469.4f}, Point{212.3f, 511.5f}, Point{155.2f, 494.2f}}},
    {{Point{219.5f, 473.3f}, Point{282.8f, 491.4f}, Point{287.4f, 534.6f}, Point{225.8f, 515.8f}}},
    {{Point{381.9f, 457.3f}, Point{460, 477.6f}, Point{457.6f, 527}, Point{382.7f, 506}}},
    {{Point{44.8f, 514.8f}, Point{92.9f, 530.8f}, Point{101.8f, 565.4f}, Point{55, 548.9f}}},
    {{Point{92.1f, 485.9f}, Point{144.9f, 502.5f}, Point{153.1f, 540.5f}, Point{101.4f, 523.5f}}},
    {{Point{157.4f, 506.4f}, Point{214, 523.9f}, Point{220, 562.7f}, Point{165, 544.4f}}},
    {{Point{298, 495.7f}, Point{366.3f, 515.6f}, Point{367.6f, 559.2f}, Point{301.5f, 539.1f}}},
    {{Point{478.6f, 482.4f}, Point{563.6f, 504.4f}, Point{556.7f, 554.8f}, Point{475.1f, 532}}},
    {{Point{584.5f, 511.2f}, Point{677.6f, 533.4f}, Point{664.8f, 581}, Point{576.5f, 561}}, ClippedCardRoundness,
        CardCorners{Point{582.5f, 512}, Point{671, 532}, Point{675.4f, 591.6f}, Point{575, 564.5f}}},
};

// The small background (187 by 138): one card, whose sides bulge a little, so its outline is measured all around; the word lies on the
// card's four corners.
constexpr CardCorners SmallBackgroundCard = {Point{27.3f, 8}, Point{179.8f, 8}, Point{150.5f, 130.3f}, Point{8.6f, 124.6f}};
const std::vector<Point> SmallBackgroundCardOutline = {{33, 8.8f}, {40.5f, 8.4f}, {52.5f, 8.1f}, {100.5f, 8}, {160.5f, 8}, {172.5f, 9.1f},
    {174.7f, 10.5f}, {175.1f, 22.5f}, {173, 34.5f}, {170.4f, 46.5f}, {167.6f, 58.5f}, {165.1f, 70.5f}, {162.1f, 82.5f}, {158.6f, 94.5f},
    {155.4f, 106.5f}, {151.6f, 118.5f}, {148.6f, 123}, {145.5f, 127}, {142, 128.2f}, {135.5f, 128.7f}, {111.5f, 128.6f}, {87.5f, 127.9f}, {63.5f, 126.9f},
    {39.5f, 125.1f}, {27.5f, 124.1f}, {15.5f, 122.6f}, {12.6f, 120.5f}, {11.9f, 108.5f}, {13.2f, 96.5f}, {15.1f, 84.5f}, {16.8f, 72.5f},
    {18.6f, 60.5f}, {20.6f, 48.5f}, {23.1f, 36.5f}, {25.9f, 24.5f}, {29.8f, 12.5f}};

float LengthOfTopSide(const CardCorners& corners)
{
    return std::hypot(corners[1].x - corners[0].x, corners[1].y - corners[0].y);
}

void DrawBackgroundWithCards(VectorCanvas& canvas, const std::vector<BackgroundCard>& cards)
{
    VectorPath background;
    background.AddRectangle(0, 0, static_cast<float>(canvas.UnitWidth()), static_cast<float>(canvas.UnitHeight()));
    VectorPath words;
    for (const BackgroundCard& card : cards)
    {
        float topSide = LengthOfTopSide(card.corners);
        CornerCuts cuts = {};
        for (std::size_t corner = 0; corner < cuts.size(); ++corner)
            cuts[corner] = topSide * card.roundness[corner];
        background.AddPath(CardShape(card.corners, cuts));
        words.AddPath(WiiWordOnCard(card.wordCard.value_or(card.corners), OnTheBackgroundCards));
    }
    canvas.Fill(background, SolidPaint(White));
    canvas.Fill(words, SolidPaint(White));
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

void DrawShopBackground(VectorCanvas& canvas)
{
    DrawBackgroundWithCards(canvas, BackgroundCards);
}

void DrawShopSmallBackground(VectorCanvas& canvas)
{
    VectorPath background;
    background.AddRectangle(0, 0, static_cast<float>(canvas.UnitWidth()), static_cast<float>(canvas.UnitHeight()));
    background.AddPolygonWithRoundCorners(SmallBackgroundCardOutline, OutlineCornerCut);
    canvas.Fill(background, SolidPaint(White));
    canvas.Fill(WiiWordOnCard(SmallBackgroundCard, OnTheSmallBackgroundCard), SolidPaint(White));
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
