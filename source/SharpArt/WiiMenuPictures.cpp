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
constexpr float DivisionUpperRightDegrees = -58.7f;

// The rim of the light part goes from the upper right point back over the top and the left side to the lower left point.
constexpr float DivisionLowerLeftDegreesOverTheTop = 147.2f - 360;

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

// The plate with the word Wii and the question mark darken the ball more.
constexpr Color MarkShadow = {0, 0, 0, 82};

void DrawRimHighlight(VectorCanvas& canvas)
{
    constexpr float Radius = RimHighlightRadius;
    VectorPath rimHighlight;
    rimHighlight.ArcTo(ButtonCenter, Radius, Radius, RimHighlightStartDegrees, RimHighlightEndDegrees);
    rimHighlight.ArcTo(RimHighlightInnerCenter, Radius, Radius, RimHighlightInnerStartDegrees, RimHighlightInnerEndDegrees);
    rimHighlight.Close();

    Paint fadingIn = RadialGradient(ButtonCenter, RimHighlightClear, RimHighlightFadeEndRadius, RimHighlight, RimHighlightFadeStartRadius);
    canvas.Fill(rimHighlight, fadingIn);
}

void DrawButtonBall(VectorCanvas& canvas, float ballRadius)
{
    VectorPath ball;
    ball.AddCircle(ButtonCenter, ballRadius);
    canvas.Fill(ball, RadialGradient(ButtonCenter, ButtonShade, ShadingRadius, ButtonShadeRim, ShadeRimStartRadius));

    VectorPath lightPart;
    lightPart.MoveTo(DivisionLowerLeft);
    lightPart.CurveTo(DivisionFirstControl, DivisionSecondControl, DivisionUpperRight);
    lightPart.ArcTo(ButtonCenter, ballRadius, ballRadius, DivisionUpperRightDegrees, DivisionLowerLeftDegreesOverTheTop);
    lightPart.Close();
    canvas.Fill(lightPart, RadialGradient(ButtonCenter, ButtonLight, ShadingRadius, ButtonLightRim, LightRimStartRadius));
    DrawRimHighlight(canvas);
}

// The ball of the Wii button of the SD Card Menu has no division: it is lit from the upper left and darkens evenly towards the lower
// right, where it turns a little blue.
void DrawEvenlyLitBall(VectorCanvas& canvas)
{
    constexpr Point LightCenter = {33, 33};
    constexpr Color Light = {235, 235, 235, 255};
    constexpr Color Middle = {214, 214, 214, 255};
    constexpr Color Darker = {206, 206, 206, 255};
    constexpr Color DarkestBlueGray = {189, 198, 198, 255};
    constexpr float LightUpTo = 20;
    constexpr float MiddleAt = 28;
    constexpr float DarkerAt = 34;
    constexpr float DarkestAt = 42;

    VectorPath ball;
    ball.AddCircle(ButtonCenter, WiiButtonRadius);
    canvas.Fill(ball, RadialGradientByDistance(LightCenter, {{LightUpTo, Light}, {MiddleAt, Middle}, {DarkerAt, Darker}, {DarkestAt, DarkestBlueGray}}));
    DrawRimHighlight(canvas);
}

// The glow ring rises from nothing to its brightest, stays there for a while and falls to nothing again, in straight steps along the
// radius.
constexpr float GlowInnerRadius = 28.6f;
constexpr float GlowBrightestStartRadius = 32.8f;
constexpr float GlowBrightestEndRadius = 34.6f;
constexpr float GlowOuterRadius = 39;
constexpr Color GlowClear = {255, 255, 255, 0};
constexpr Color GlowBrightest = {255, 255, 255, 245};

void FillRing(VectorCanvas& canvas, float innerRadius, float outerRadius, const Paint& paint)
{
    VectorPath ring;
    ring.AddCircle(ButtonCenter, outerRadius);
    ring.AddCircle(ButtonCenter, innerRadius);
    canvas.Fill(ring, paint);
}

// The SD card: three outlines of the card, each a unit inside the one before, then the label, the bands below it and the mark.
constexpr float CardCornerCut = 0.8f;
constexpr Color CardEdgeBlue = {49, 189, 239, 255};
constexpr Color CardInnerEdgeTopBlue = {123, 198, 231, 255};
constexpr Color CardInnerEdgeBottomBlue = {90, 173, 206, 255};
constexpr Color CardBodyBlue = {181, 222, 239, 255};
constexpr Color CardSideBlue = {173, 214, 231, 255};
constexpr Color CardLabelWhite = {255, 255, 255, 255};
constexpr Color CardLineUnderTheLabel = {156, 206, 222, 255};
constexpr Color CardBandBlue = {82, 198, 231, 255};
constexpr Color CardLowerStrip = {173, 198, 214, 255};
constexpr Color CardBottomBlue = {90, 165, 198, 255};
constexpr Color SdMarkBlue = {165, 214, 236, 255};
constexpr Color SdTickBlue = {148, 206, 231, 150};

std::vector<Point> CardOutline(float inset)
{
    constexpr float Left = 2;
    constexpr float Top = 2;
    constexpr float Right = 36;
    constexpr float Bottom = 46;
    constexpr float CutLength = 4.4f;

    // When the outline moves inward, the ends of the cut corner of 45 degrees move along the sides by tan(22.5 degrees) as much.
    constexpr float CutCornerShift = 0.414f;

    float cutInset = inset * CutCornerShift;
    return {
        {Left + inset, Top + inset},
        {Right - CutLength - cutInset, Top + inset},
        {Right - inset, Top + CutLength + cutInset},
        {Right - inset, Bottom - inset},
        {Left + inset, Bottom - inset},
    };
}

void FillCardOutline(VectorCanvas& canvas, float inset, const Paint& paint)
{
    VectorPath card;
    card.AddPolygonWithRoundCorners(CardOutline(inset), CardCornerCut);
    canvas.Fill(card, paint);
}

void FillRectangle(VectorCanvas& canvas, float left, float top, float right, float bottom, const Paint& paint)
{
    VectorPath rectangle;
    rectangle.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(rectangle, paint);
}

void FillPolygon(VectorCanvas& canvas, std::initializer_list<Point> corners, Color color)
{
    VectorPath polygon;
    polygon.AddPolygon(corners);
    canvas.Fill(polygon, SolidPaint(color));
}

// The S of the mark: a top bar, a slanted middle stroke and a bottom bar, all leaning to the right.
void DrawLetterSOfTheMark(VectorCanvas& canvas)
{
    FillPolygon(canvas, {{11, 19.2f}, {19.9f, 19.2f}, {19.5f, 20.9f}, {10, 20.9f}}, SdMarkBlue);
    FillPolygon(canvas, {{10, 20.5f}, {14, 20.5f}, {17.9f, 24.2f}, {17.6f, 26.5f}, {12.8f, 23.4f}, {10, 23}}, SdMarkBlue);
    FillPolygon(canvas, {{8.2f, 24.2f}, {17.9f, 24.2f}, {17.6f, 26.6f}, {7.6f, 26.6f}}, SdMarkBlue);
}

// The D of the mark: a slanted round shape with a slit that opens to the left.
void DrawLetterDOfTheMark(VectorCanvas& canvas)
{
    constexpr float RoundCut = 1.5f;

    VectorPath letter;
    letter.AddPolygonWithRoundCorners({{20.6f, 19.2f}, {27, 19.6f}, {30.4f, 22.4f}, {26.4f, 26.6f}, {18.2f, 26.6f}}, RoundCut);
    canvas.Fill(letter, SolidPaint(SdMarkBlue));
    FillPolygon(canvas, {{17.5f, 20.9f}, {23.3f, 20.9f}, {23, 22.8f}, {17, 22.8f}}, CardLabelWhite);
}

// The disc: every ring is drawn with the hole cut out of it, so the hole stays clear.
constexpr Point DiscCenter = {48, 48};
constexpr float DiscHoleRadius = 4.2f;

Color DiscGray(Formats::Byte level)
{
    return {level, level, level, 255};
}

void FillDiscRing(VectorCanvas& canvas, float outerRadius, const Paint& paint)
{
    VectorPath ring;
    ring.AddCircle(DiscCenter, outerRadius);
    ring.AddCircle(DiscCenter, DiscHoleRadius);
    canvas.Fill(ring, paint);
}

// The shine of the disc by direction, in degrees from the right going down: two light and two dark sectors, with small turns between.
const std::vector<ColorStop> DiscShineByDegrees = {
    {0, DiscGray(156)},
    {15, DiscGray(163)},
    {30, DiscGray(188)},
    {45, DiscGray(206)},
    {60, DiscGray(211)},
    {75, DiscGray(203)},
    {90, DiscGray(200)},
    {105, DiscGray(221)},
    {120, DiscGray(201)},
    {135, DiscGray(159)},
    {150, DiscGray(144)},
    {165, DiscGray(136)},
    {180, DiscGray(129)},
    {195, DiscGray(164)},
    {210, DiscGray(147)},
    {225, DiscGray(118)},
    {240, DiscGray(142)},
    {255, DiscGray(182)},
    {270, DiscGray(204)},
    {285, DiscGray(184)},
    {300, DiscGray(143)},
    {315, DiscGray(118)},
    {330, DiscGray(156)},
    {345, DiscGray(188)},
    {360, DiscGray(156)},
};

const std::vector<ColorStop> DiscEdgeByDegrees = {
    {0, DiscGray(139)},
    {45, DiscGray(193)},
    {90, DiscGray(194)},
    {180, DiscGray(139)},
    {225, DiscGray(167)},
    {270, DiscGray(176)},
    {360, DiscGray(139)},
};

// Towards the hub the shine is softer: a middle gray lies over it, fading out towards the rim.
constexpr Color DiscMiddleGray = {160, 160, 160, 130};
constexpr Color DiscMiddleGrayClear = {160, 160, 160, 0};

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

// The Wii button of the SD Card Menu and the Message Board: the word Wii is cut out of a dark plate that darkens the ball under it.
void DrawWiiPlateButton(VectorCanvas& canvas)
{
    constexpr float PlateLeft = 18.2f;
    constexpr float PlateTop = 23.3f;
    constexpr float PlateRight = 62;
    constexpr float PlateBottom = 56.8f;
    constexpr float PlateCornerRadius = 3.5f;
    constexpr Point PlateWordOffset = {20.3f, 28.75f};
    constexpr float PlateWordScale = 0.59f;

    DrawEvenlyLitBall(canvas);

    VectorPath plate;
    plate.AddRoundedRectangle(PlateLeft, PlateTop, PlateRight - PlateLeft, PlateBottom - PlateTop, PlateCornerRadius);
    AddWiiWord(plate, PlateWordOffset, PlateWordScale);
    canvas.Fill(plate, SolidPaint(MarkShadow));
}

// The help button of the SD Card Menu: a question mark that darkens the ball. Its hook is one outline: the outer edge of the arc, the
// slant down to the stem, the stem, and back up along the inner edge of the arc.
void DrawQuestionButton(VectorCanvas& canvas)
{
    constexpr Point HookCenter = {40.1f, 33.6f};
    constexpr float OuterRadiusX = 9.6f;
    constexpr float OuterRadiusY = 9;
    constexpr float InnerRadiusX = 5.6f;
    constexpr float InnerRadiusY = 5;
    constexpr float HookStartDegrees = 180;
    constexpr float HookEndDegrees = 400;
    constexpr float StemLeft = 38;
    constexpr float StemRight = 42.3f;
    constexpr float StemBottom = 47.8f;
    constexpr float DotTop = 50.6f;
    constexpr float DotBottom = 56;

    DrawButtonBall(canvas, WiiButtonRadius);

    VectorPath mark;
    mark.ArcTo(HookCenter, OuterRadiusX, OuterRadiusY, HookStartDegrees, HookEndDegrees);
    mark.LineTo({StemRight, 44});
    mark.LineTo({StemRight, StemBottom});
    mark.LineTo({StemLeft, StemBottom});
    mark.LineTo({StemLeft, 42.6f});
    mark.LineTo({40.2f, 39.6f});
    mark.ArcTo(HookCenter, InnerRadiusX, InnerRadiusY, HookEndDegrees, HookStartDegrees);
    mark.Close();
    mark.AddRectangle(StemLeft, DotTop, StemRight - StemLeft, DotBottom - DotTop);
    canvas.Fill(mark, SolidPaint(MarkShadow));
}

void DrawButtonGlowRing(VectorCanvas& canvas)
{
    Paint glow = RadialGradientByDistance(ButtonCenter, {
        {GlowInnerRadius, GlowClear},
        {GlowBrightestStartRadius, GlowBrightest},
        {GlowBrightestEndRadius, GlowBrightest},
        {GlowOuterRadius, GlowClear},
    });
    FillRing(canvas, GlowInnerRadius, GlowOuterRadius, glow);
}

void DrawSdCard(VectorCanvas& canvas)
{
    FillCardOutline(canvas, 0, SolidPaint(CardEdgeBlue));
    FillCardOutline(canvas, 1, LinearGradient({0, 3}, CardInnerEdgeTopBlue, {0, 45}, CardInnerEdgeBottomBlue));
    FillCardOutline(canvas, 2, SolidPaint(CardBodyBlue));

    FillRectangle(canvas, 4, 9, 5, 44, SolidPaint(CardSideBlue));
    FillRectangle(canvas, 33, 8, 34, 44, SolidPaint(CardSideBlue));

    FillRectangle(canvas, 6, 10, 32, 35.7f, SolidPaint(CardLabelWhite));
    FillRectangle(canvas, 5, 35.7f, 33, 37, SolidPaint(CardLineUnderTheLabel));
    FillRectangle(canvas, 5, 37, 33, 42, SolidPaint(CardBandBlue));
    FillRectangle(canvas, 4, 42, 34, 44, SolidPaint(CardLowerStrip));
    FillRectangle(canvas, 3, 44, 35, 45, SolidPaint(CardBottomBlue));

    FillRectangle(canvas, 7.1f, 11, 7.9f, 14, SolidPaint(SdTickBlue));
    DrawLetterSOfTheMark(canvas);
    DrawLetterDOfTheMark(canvas);
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

void DrawDiscChannelDisc(VectorCanvas& canvas)
{
    constexpr float EdgeRadius = 41.5f;
    constexpr float RimRadius = 40.2f;
    constexpr float ShineRadius = 37.6f;
    constexpr float SoftShineRadius = 12;
    constexpr float HubOuterRadius = 13;
    constexpr float HubWhiteRingRadius = 11.8f;
    constexpr float HubGrayRingRadius = 10.5f;
    constexpr float HubInnerWhiteRadius = 9.4f;
    constexpr float HubInnerEdgeRadius = 5.8f;

    FillDiscRing(canvas, EdgeRadius, ConicGradientByDegrees(DiscCenter, DiscEdgeByDegrees));
    FillDiscRing(canvas, RimRadius, SolidPaint(DiscGray(246)));
    FillDiscRing(canvas, ShineRadius, ConicGradientByDegrees(DiscCenter, DiscShineByDegrees));
    FillDiscRing(canvas, ShineRadius, RadialGradient(DiscCenter, DiscMiddleGray, ShineRadius, DiscMiddleGrayClear, SoftShineRadius));

    FillDiscRing(canvas, HubOuterRadius, SolidPaint(DiscGray(203)));
    FillDiscRing(canvas, HubWhiteRingRadius, SolidPaint(DiscGray(251)));
    FillDiscRing(canvas, HubGrayRingRadius, SolidPaint(DiscGray(150)));
    FillDiscRing(canvas, HubInnerWhiteRadius, SolidPaint(DiscGray(255)));
    FillDiscRing(canvas, HubInnerEdgeRadius, SolidPaint(DiscGray(190)));
}

}
