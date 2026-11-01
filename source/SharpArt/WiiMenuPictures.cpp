#include "SharpArt/WiiMenuPictures.h"

#include "SharpArt/WiiLogoPictures.h"

#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};

// The outline of the blue arrow, and the same outline a little inside it, where the light blue begins.
struct ArrowCorners
{
    Point top;
    Point tip;
    Point bottom;
    Point notch;
};

constexpr ArrowCorners ArrowOutline = {{18, 8.4f}, {43.3f, 32}, {18, 55.6f}, {22.2f, 32}};
constexpr ArrowCorners ArrowInside = {{19.55f, 11.2f}, {41.85f, 32}, {19.55f, 52.8f}, {23.2f, 32}};

// The inside is light along the middle line of the arrow and deeper towards the top and the bottom.
constexpr Color ArrowEdgeBlue = {0, 41, 140, 255};
constexpr Color ArrowLightBlue = {173, 239, 255, 255};
constexpr Color ArrowDeepBlue = {90, 189, 239, 255};
constexpr float ArrowMiddleY = 32;
constexpr float ArrowTopY = 12;
constexpr float ArrowBottomY = 52;
constexpr float ArrowCornerCut = 1.6f;

void FillArrowShape(VectorCanvas& canvas, const ArrowCorners& corners, const Paint& paint)
{
    VectorPath arrow;
    arrow.AddPolygonWithRoundCorners({corners.top, corners.tip, corners.bottom, corners.notch}, ArrowCornerCut);
    canvas.Fill(arrow, paint);
}

void FillArrowInside(VectorCanvas& canvas, const ArrowCorners& corners)
{
    Point middleOfTheLeft = {corners.notch.x, ArrowMiddleY};

    VectorPath upperHalf;
    upperHalf.AddPolygon({corners.top, corners.tip, middleOfTheLeft, corners.notch});
    canvas.Fill(upperHalf, LinearGradient({0, ArrowMiddleY}, ArrowLightBlue, {0, ArrowTopY}, ArrowDeepBlue));

    VectorPath lowerHalf;
    lowerHalf.AddPolygon({corners.notch, corners.tip, corners.bottom});
    canvas.Fill(lowerHalf, LinearGradient({0, ArrowMiddleY}, ArrowLightBlue, {0, ArrowBottomY}, ArrowDeepBlue));
}

ArrowCorners MirroredLeftToRight(const ArrowCorners& corners, float pictureWidth)
{
    auto mirror = [pictureWidth](Point point) { return Point{pictureWidth - point.x, point.y}; };
    return {mirror(corners.top), mirror(corners.tip), mirror(corners.bottom), mirror(corners.notch)};
}

// The round button: a gray ball, light in the upper left and darker in the lower right, the two parts divided by a soft curve, with a
// bright rim in the upper left. Angles are in degrees, 0 to the right and 90 down.
constexpr Point ButtonCenter = {40, 40};

// The Wii button is a little smaller than the plain button; their shading is the same.
constexpr float WiiButtonRadius = 35.45f;
constexpr float RoundButtonRadius = 37.45f;
constexpr float ShadingRadius = WiiButtonRadius;
constexpr Color ButtonLight = {230, 230, 230, 255};
constexpr Color ButtonLightRim = {208, 208, 208, 255};
constexpr Color ButtonShade = {213, 213, 213, 255};
constexpr Color ButtonShadeRim = {190, 190, 190, 255};
constexpr float LightRimStartRadius = 29;
constexpr float ShadeRimStartRadius = 21;

// Where the curve between the light and the shaded part meets the rim, and its control points.
constexpr Point DivisionLowerLeft = {10.5f, 59};
constexpr Point DivisionFirstControl = {34, 54};
constexpr Point DivisionSecondControl = {58, 34};
constexpr Point DivisionUpperRight = {58.5f, 9.6f};
constexpr float DivisionLowerLeftDegrees = 147.2f;
constexpr float DivisionUpperRightDegrees = -58.7f;

// The bright line a little inside the rim: between a circle inside the edge of the ball and a circle of the same size moved down and
// right, so it is widest in the upper left and runs out at both ends. It fades in from the inside.
constexpr Color RimHighlight = {242, 242, 242, 255};
constexpr Color RimHighlightClear = {242, 242, 242, 0};
constexpr float RimHighlightRadius = 33;
constexpr Point RimHighlightInnerCenter = {42.5f, 42.5f};
constexpr float RimHighlightStartDegrees = 131.9f;
constexpr float RimHighlightEndDegrees = 318.1f;
constexpr float RimHighlightInnerStartDegrees = 311.9f;
constexpr float RimHighlightInnerEndDegrees = 138.1f;
constexpr float RimHighlightFadeStartRadius = 25;
constexpr float RimHighlightFadeEndRadius = 31;

// The word Wii darkens the ball under it, the light and the shaded part alike.
constexpr Color WordShadow = {0, 0, 0, 59};
constexpr Point WordOffset = {18.22f, 27.95f};
constexpr float WordScale = 0.61f;

void DrawButtonBall(VectorCanvas& canvas, float ballRadius)
{
    VectorPath ball;
    ball.AddCircle(ButtonCenter, ballRadius);
    canvas.Fill(ball, RadialGradient(ButtonCenter, ButtonShade, ShadingRadius, ButtonShadeRim, ShadeRimStartRadius));

    VectorPath lightPart;
    lightPart.MoveTo(DivisionLowerLeft);
    lightPart.CurveTo(DivisionFirstControl, DivisionSecondControl, DivisionUpperRight);
    lightPart.ArcTo(ButtonCenter, ballRadius, ballRadius, DivisionUpperRightDegrees, DivisionLowerLeftDegrees - 360);
    lightPart.Close();
    canvas.Fill(lightPart, RadialGradient(ButtonCenter, ButtonLight, ShadingRadius, ButtonLightRim, LightRimStartRadius));

    VectorPath rimHighlight;
    rimHighlight.ArcTo(ButtonCenter, RimHighlightRadius, RimHighlightRadius, RimHighlightStartDegrees, RimHighlightEndDegrees);
    rimHighlight.ArcTo(RimHighlightInnerCenter, RimHighlightRadius, RimHighlightRadius, RimHighlightInnerStartDegrees, RimHighlightInnerEndDegrees);
    rimHighlight.Close();
    canvas.Fill(rimHighlight, RadialGradient(ButtonCenter, RimHighlightClear, RimHighlightFadeEndRadius, RimHighlight, RimHighlightFadeStartRadius));
}

void DrawArrow(VectorCanvas& canvas, const ArrowCorners& outline, const ArrowCorners& inside)
{
    FillArrowShape(canvas, outline, SolidPaint(ArrowEdgeBlue));
    FillArrowInside(canvas, inside);
}

}

void DrawEnvelope(VectorCanvas& canvas)
{
    constexpr float Left = 3.27f;
    constexpr float Right = 45.73f;
    constexpr float Top = 2.8f;
    constexpr float Bottom = 30.2f;
    constexpr float CornerRadius = 1.2f;

    // The edge of the flap: a thin band from the sides of the envelope down to a point in the middle.
    constexpr float FlapSideY = 9.5f;
    constexpr float FlapPointX = 24.5f;
    constexpr float FlapPointY = 19.5f;
    constexpr float HalfBandHeight = 0.85f;
    constexpr float HalfBandHeightAtThePoint = 1.2f;

    VectorPath envelope;
    envelope.AddRoundedRectangle(Left, Top, Right - Left, Bottom - Top, CornerRadius);
    envelope.AddPolygon({
        {Left, FlapSideY - HalfBandHeight},
        {FlapPointX, FlapPointY - HalfBandHeightAtThePoint},
        {Right, FlapSideY - HalfBandHeight},
        {Right, FlapSideY + HalfBandHeight},
        {FlapPointX, FlapPointY + HalfBandHeightAtThePoint},
        {Left, FlapSideY + HalfBandHeight},
    });
    canvas.Fill(envelope, SolidPaint(White));
}

void DrawRoundButton(VectorCanvas& canvas)
{
    DrawButtonBall(canvas, RoundButtonRadius);
}

void DrawWiiButton(VectorCanvas& canvas)
{
    DrawButtonBall(canvas, WiiButtonRadius);

    VectorPath word;
    AddWiiWord(word, WordOffset, WordScale);
    canvas.Fill(word, SolidPaint(WordShadow));
}

void DrawArrowRight(VectorCanvas& canvas)
{
    DrawArrow(canvas, ArrowOutline, ArrowInside);
}

void DrawArrowLeft(VectorCanvas& canvas)
{
    float width = static_cast<float>(canvas.UnitWidth());
    DrawArrow(canvas, MirroredLeftToRight(ArrowOutline, width), MirroredLeftToRight(ArrowInside, width));
}

}
