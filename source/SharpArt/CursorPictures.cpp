#include "SharpArt/CursorPictures.h"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color Black = {0, 0, 0, 255};
constexpr Color White = {255, 255, 255, 255};

// ---- The hands

// The black edge around the white of the hand.
constexpr float EdgeWidth = 3.2f;

// The white palm has slightly round corners.
constexpr float PalmCornerCut = 1.5f;

// A finger is a straight band with a round tip: half a circle around the middle of the tip, then straight sides down to its base.
struct Finger
{
    Point tipCenter;
    Point baseCenter;
    float halfWidth = 0;
};

// The white parts of a hand: the fingers and the palm with the thumb. The black edge lies around all of them; where two white parts
// do not meet, the edge between them is a black line.
struct HandShape
{
    std::vector<Finger> fingers;
    std::vector<Point> palmCorners;
};

// Directions in degrees: 0 to the right, 90 down, 180 to the left, 270 up (y grows downward).
constexpr float HalfTurnDegrees = 180;

float DegreesOf(Point direction)
{
    return std::atan2(direction.y, direction.x) * HalfTurnDegrees / std::numbers::pi_v<float>;
}

VectorPath FingerPath(const Finger& finger, float halfWidth)
{
    float length = std::hypot(finger.baseCenter.x - finger.tipCenter.x, finger.baseCenter.y - finger.tipCenter.y);
    Point down = {(finger.baseCenter.x - finger.tipCenter.x) / length, (finger.baseCenter.y - finger.tipCenter.y) / length};
    Point side = {-down.y * halfWidth, down.x * halfWidth};
    float sideDegrees = DegreesOf(side);

    VectorPath path;
    path.MoveTo({finger.baseCenter.x + side.x, finger.baseCenter.y + side.y});
    path.ArcTo(finger.tipCenter, halfWidth, halfWidth, sideDegrees, sideDegrees + HalfTurnDegrees);
    path.LineTo({finger.baseCenter.x - side.x, finger.baseCenter.y - side.y});
    path.Close();
    return path;
}

VectorPath PalmPath(const HandShape& hand)
{
    VectorPath path;
    path.AddPolygonWithRoundCorners(hand.palmCorners, PalmCornerCut);
    return path;
}

// The palm with a round pen of the width of the edge drawn along its outline, which makes it larger by the edge on every side.
void FillPalmWithEdge(VectorCanvas& canvas, const HandShape& hand)
{
    VectorPath palm = PalmPath(hand);
    canvas.Fill(palm, SolidPaint(Black));

    const Outline& outline = palm.Outlines().front();
    for (std::size_t index = 0; index < outline.size(); ++index)
    {
        VectorPath line;
        line.AddStroke(outline[index], outline[(index + 1) % outline.size()], 2 * EdgeWidth);
        canvas.Fill(line, SolidPaint(Black));
    }
}

void DrawHand(VectorCanvas& canvas, const HandShape& hand)
{
    for (const Finger& finger : hand.fingers)
        canvas.Fill(FingerPath(finger, finger.halfWidth + EdgeWidth), SolidPaint(Black));
    FillPalmWithEdge(canvas, hand);

    for (const Finger& finger : hand.fingers)
        canvas.Fill(FingerPath(finger, finger.halfWidth), SolidPaint(White));
    canvas.Fill(PalmPath(hand), SolidPaint(White));
}

// The fingers of the pointing and the grabbing hand end in the palm at this height.
constexpr float FingerBaseY = 31;

constexpr Finger MiddleFinger = {{31.5f, 26.6f}, {31.5f, FingerBaseY}, 3.3f};
constexpr Finger RingFinger = {{39.05f, 27.85f}, {39.05f, FingerBaseY}, 2.85f};
constexpr Finger LittleFinger = {{46, 29.8f}, {46, FingerBaseY}, 2.8f};

// The palm of the pointing and the grabbing hand, with the thumb on the left: the line between the thumb and the index finger is the
// narrow notch at the top left.
const std::vector<Point> ClosedHandPalm = {
    {19.4f, 29.6f},
    {48.8f, 29.6f},
    {48.8f, 47},
    {44.3f, 55},
    {44.1f, 58.8f},
    {23.2f, 58.8f},
    {23.2f, 55.2f},
    {15, 47.2f},
    {13.8f, 44.5f},
    {13.8f, 33.5f},
    {16.6f, 30.8f},
    {17.6f, 30.8f},
    {17.6f, 37.4f},
    {19.4f, 37.4f},
};

// The open hand: four spread fingers, and the palm reaches down along the thumb, which is stretched out to the lower left.
const std::vector<Point> OpenHandPalm = {
    {19.3f, 29.5f},
    {51, 29.5f},
    {50.2f, 45.5f},
    {45.2f, 55.5f},
    {45.2f, 58.8f},
    {23.4f, 58.8f},
    {23.3f, 54.5f},
    {19.5f, 50.3f},
    {15, 47.6f},
    {17.6f, 38},
    {19.3f, 37.6f},
};

}

void DrawPointingHand(VectorCanvas& canvas)
{
    constexpr Finger IndexFinger = {{23.15f, 8.55f}, {23.15f, FingerBaseY}, 3.65f};
    DrawHand(canvas, {{IndexFinger, MiddleFinger, RingFinger, LittleFinger}, ClosedHandPalm});
}

void DrawGrabbingHand(VectorCanvas& canvas)
{
    constexpr Finger BentIndexFinger = {{23, 26.8f}, {23, FingerBaseY}, 3.5f};
    DrawHand(canvas, {{BentIndexFinger, MiddleFinger, RingFinger, LittleFinger}, ClosedHandPalm});
}

void DrawOpenHand(VectorCanvas& canvas)
{
    constexpr float OpenFingerBaseY = 31;
    constexpr Finger IndexFinger = {{21, 11.9f}, {23.5f, OpenFingerBaseY}, 3.7f};
    constexpr Finger SpreadMiddleFinger = {{32.1f, 10.6f}, {32.1f, OpenFingerBaseY}, 3.4f};
    constexpr Finger SpreadRingFinger = {{41.6f, 13}, {40.2f, OpenFingerBaseY}, 3.2f};
    constexpr Finger SpreadLittleFinger = {{49.2f, 19.9f}, {48.1f, OpenFingerBaseY}, 2.85f};
    constexpr Finger Thumb = {{9.9f, 33.3f}, {18.8f, 45.4f}, 3.85f};
    DrawHand(canvas, {{IndexFinger, SpreadMiddleFinger, SpreadRingFinger, SpreadLittleFinger, Thumb}, OpenHandPalm});
}

// ---- The shading of the hands and the numbers of the players

namespace
{

constexpr Color ShadeOfTheWrist = {194, 194, 194, 255};

// The shading is white down to the start of the palm and turns gray towards the wrist.
void FillShade(VectorCanvas& canvas, float whiteDownTo, float grayFrom, Color wristGray)
{
    VectorPath whole;
    whole.AddRectangle(0, 0, static_cast<float>(canvas.UnitWidth()), static_cast<float>(canvas.UnitHeight()));
    canvas.Fill(whole, LinearGradient({0, whiteDownTo}, White, {0, grayFrom}, wristGray));
}

void FillHandShade(VectorCanvas& canvas)
{
    constexpr float WhiteDownTo = 37.5f;
    constexpr float GrayFrom = 47;
    FillShade(canvas, WhiteDownTo, GrayFrom, ShadeOfTheWrist);
}

// The numbers are cut out of the shading, so the color of the player shows through: they are painted black, which is no light at all.
// They are bold digits about 18 units high in the middle of the palm, with slightly round corners. An outline inside another one is a hole.
constexpr float DigitCornerCut = 0.9f;

void FillDigit(VectorCanvas& canvas, const std::vector<std::vector<Point>>& outlines)
{
    VectorPath digit;
    for (const std::vector<Point>& outline : outlines)
        digit.AddPolygonWithRoundCorners(outline, DigitCornerCut);
    canvas.Fill(digit, SolidPaint(Black));
}

}

void DrawHandShadeWithoutNumber(VectorCanvas& canvas)
{
    constexpr float WhiteDownTo = 17.5f;
    constexpr float GrayFrom = 23;
    constexpr Color GrayOfTheSmallShade = {170, 170, 170, 255};
    FillShade(canvas, WhiteDownTo, GrayFrom, GrayOfTheSmallShade);
}

void DrawHandShadeOfPlayerOne(VectorCanvas& canvas)
{
    FillHandShade(canvas);
    FillDigit(canvas, {{{29.5f, 33.8f}, {36, 33.8f}, {36, 51}, {32, 51}, {32, 38}, {29.5f, 37.5f}}});
}

void DrawHandShadeOfPlayerTwo(VectorCanvas& canvas)
{
    FillHandShade(canvas);
    FillDigit(canvas, {{{27.3f, 39.3f}, {27.3f, 36.5f}, {30.2f, 33.6f}, {35.8f, 33.6f}, {39.3f, 37}, {39.3f, 40.5f}, {35, 46.6f},
        {39.4f, 46.6f}, {39.4f, 50.9f}, {27.1f, 50.9f}, {27.1f, 46.6f}, {34.4f, 39.3f}, {34.2f, 37.4f}, {31.6f, 37.4f}, {31.4f, 39.3f}}});
}

void DrawHandShadeOfPlayerThree(VectorCanvas& canvas)
{
    FillHandShade(canvas);
    FillDigit(canvas, {{{27.4f, 38.3f}, {27.4f, 36.5f}, {30.2f, 33.5f}, {35.6f, 33.5f}, {39.2f, 36.8f}, {39.3f, 40}, {38.6f, 42},
        {39.7f, 44.5f}, {39.7f, 47.5f}, {36.3f, 51.3f}, {29.5f, 51.3f}, {26.3f, 48.3f}, {26.3f, 45.3f}, {30.7f, 45.3f}, {31.3f, 46.9f},
        {34.7f, 46.9f}, {34.7f, 44.4f}, {31.3f, 44.4f}, {31.3f, 40}, {34.3f, 40}, {34.3f, 37.4f}, {31.4f, 37.4f}, {31.2f, 38.3f}}});
}

void DrawHandShadeOfPlayerFour(VectorCanvas& canvas)
{
    FillHandShade(canvas);
    FillDigit(canvas, {{{25.5f, 44}, {33.8f, 33.8f}, {37, 33.8f}, {37, 44}, {40, 44}, {40, 47.9f}, {37, 47.9f}, {37, 51}, {33, 51},
                           {33, 47.9f}, {25.5f, 47.9f}},
                          {{31, 44}, {33, 41.4f}, {33, 44}}});
}

}
