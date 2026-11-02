#include "SharpArt/HomeMenuPictures.h"

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

Paint VerticalGradientByHeight(const std::vector<ColorStop>& stopsByHeight)
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

void FillRectangle(VectorCanvas& canvas, float left, float top, float right, float bottom, const Paint& paint)
{
    VectorPath rectangle;
    rectangle.AddRectangle(left, top, right - left, bottom - top);
    canvas.Fill(rectangle, paint);
}

void FillCircle(VectorCanvas& canvas, Point center, float radius, const Paint& paint)
{
    VectorPath circle;
    circle.AddCircle(center, radius);
    canvas.Fill(circle, paint);
}

// ---- The big button

// Each end of the big button is half a circle around a point on the seam with the middle strip. The dark rim lies around a body
// that is shaded from the upper left: a light band along the upper edge, a lighter glossy patch in the upper half and a darker edge
// below and on the right.
constexpr float BigButtonMiddleY = 56;
constexpr float BigButtonRimRadius = 52.2f;
constexpr float BigButtonBodyRadius = 50;
constexpr float LeftEndCenterX = 55.4f;
constexpr float RightEndCenterX = 0.2f;
constexpr Color BigButtonRim = {89, 89, 89, 255};

// The darker edge of the body: the body keeps its gray up to a distance from a point above the middle and then darkens.
constexpr float ShadeCenterY = 48.6f;
constexpr float RightEndShadeShiftX = -5.2f;
const std::vector<ColorStop> BodyShadeByDistance = {
    {44, Gray(203)},
    {46, Gray(199)},
    {48, Gray(188)},
    {50, Gray(175)},
    {52, Gray(162)},
    {54, Gray(152)},
    {56, Gray(147)},
};

// The light band along the edge changes its brightness around the end: brightest towards the upper left, and it fades out in the
// lower left of the left end and in the upper right of the right end.
const std::vector<ColorStop> LeftEndBandByDegrees = {
    {138, ClearGray(205)},
    {180, Gray(205)},
    {225, Gray(231)},
    {270, Gray(224)},
};
const std::vector<ColorStop> RightEndBandByDegrees = {
    {0, ClearGray(205)},
    {90, ClearGray(205)},
    {270, Gray(224)},
    {300, Gray(210)},
    {330, ClearGray(205)},
};

// The glossy patch: inside a circle a little smaller than the body, above a soft curve through the middle of the button. It is
// brightest along its upper edge.
constexpr float HighlightCenterY = 55.5f;
constexpr float HighlightRadius = 43.7f;
constexpr float HighlightTop = HighlightCenterY - HighlightRadius;
constexpr float HighlightBottomAtTheSeams = 47;
const std::vector<ColorStop> HighlightByDistance = {
    {35, Gray(225)},
    {38, Gray(227)},
    {41, Gray(229)},
    {HighlightRadius, Gray(231)},
};

// The curve under the patch of the left end goes down to its lowest point and turns up into the circle of the patch.
constexpr Point LeftHighlightLowestPoint = {23, 59.5f};
constexpr float LeftHighlightTurnDegrees = 188;

// The patch of the right end ends in a straight side with round corners.
constexpr float RightHighlightSideX = 31.7f;
constexpr float RightHighlightSideTop = 30;
constexpr float RightHighlightCornerDegrees = 310;

Point ShadeCenterOfLeftEnd()
{
    return {LeftEndCenterX, ShadeCenterY};
}

Point ShadeCenterOfRightEnd()
{
    return {RightEndCenterX + RightEndShadeShiftX, ShadeCenterY};
}

void DrawBigButtonEndBody(VectorCanvas& canvas, float centerX, Point shadeCenter, const std::vector<ColorStop>& bandByDegrees)
{
    Point center = {centerX, BigButtonMiddleY};
    FillCircle(canvas, center, BigButtonRimRadius, SolidPaint(BigButtonRim));
    FillCircle(canvas, center, BigButtonBodyRadius, RadialGradientByDistance(shadeCenter, BodyShadeByDistance));

    VectorPath band;
    band.AddCircle(center, BigButtonBodyRadius);
    band.AddCircle({centerX, HighlightCenterY}, HighlightRadius);
    canvas.Fill(band, ConicGradientByDegrees(center, bandByDegrees));
}

Paint HighlightPaint(float centerX)
{
    return RadialGradientByDistance({centerX, HighlightCenterY}, HighlightByDistance);
}

void DrawLeftEndHighlight(VectorCanvas& canvas)
{
    constexpr float SeamX = 56;
    Point highlightCenter = {LeftEndCenterX, HighlightCenterY};

    VectorPath highlight;
    highlight.MoveTo({SeamX, HighlightBottomAtTheSeams});
    highlight.CurveTo({42, 46.5f}, {30, 59.5f}, LeftHighlightLowestPoint);
    highlight.CurveTo({16.5f, 59.5f}, {12.1f, 55}, {12.13f, 49.42f});
    highlight.ArcTo(highlightCenter, HighlightRadius, HighlightRadius, LeftHighlightTurnDegrees, 270);
    highlight.LineTo({SeamX, HighlightTop});
    highlight.Close();
    canvas.Fill(highlight, HighlightPaint(LeftEndCenterX));
}

void DrawRightEndHighlight(VectorCanvas& canvas)
{
    constexpr float SeamX = 0;
    Point highlightCenter = {RightEndCenterX, HighlightCenterY};

    VectorPath highlight;
    highlight.MoveTo({SeamX, HighlightBottomAtTheSeams});
    highlight.CurveTo({14, 45}, {RightHighlightSideX, 38}, {RightHighlightSideX, RightHighlightSideTop});
    highlight.CurveTo({RightHighlightSideX, 26}, {30.5f, 24}, {28.3f, 22});
    highlight.ArcTo(highlightCenter, HighlightRadius, HighlightRadius, RightHighlightCornerDegrees, 270);
    highlight.LineTo({SeamX, HighlightTop});
    highlight.Close();
    canvas.Fill(highlight, HighlightPaint(RightEndCenterX));
}

// The middle strip is the cut through both ends at their seams: every column is the same.
void DrawBigButtonMiddleColumn(VectorCanvas& canvas)
{
    float width = static_cast<float>(canvas.UnitWidth());
    float rimTop = BigButtonMiddleY - BigButtonRimRadius;
    float rimBottom = BigButtonMiddleY + BigButtonRimRadius;
    float bodyTop = BigButtonMiddleY - BigButtonBodyRadius;
    float bodyBottom = BigButtonMiddleY + BigButtonBodyRadius;

    FillRectangle(canvas, 0, rimTop, width, rimBottom, SolidPaint(BigButtonRim));

    std::vector<ColorStop> shadeByHeight;
    for (const ColorStop& stop : BodyShadeByDistance)
        shadeByHeight.push_back({ShadeCenterY + stop.share, stop.color});
    FillRectangle(canvas, 0, bodyTop, width, bodyBottom, VerticalGradientByHeight(shadeByHeight));

    FillRectangle(canvas, 0, bodyTop, width, HighlightTop, SolidPaint(Gray(224)));

    std::vector<ColorStop> highlightByHeight;
    for (auto stop = HighlightByDistance.rbegin(); stop != HighlightByDistance.rend(); ++stop)
        highlightByHeight.push_back({HighlightCenterY - stop->share, stop->color});
    highlightByHeight.push_back({HighlightBottomAtTheSeams, Gray(225)});
    FillRectangle(canvas, 0, HighlightTop, width, HighlightBottomAtTheSeams, VerticalGradientByHeight(highlightByHeight));
}

// ---- The buttons of the remote settings

// The button is drawn whole, 56 wide: the left end shows its first 24 units, the middle strip the 8 after them and the right end the
// last 24. Every column of the middle strip is the same, so the menu can stretch it.
constexpr float SettingsButtonWidth = 56;
constexpr float SettingsButtonMiddleStart = 24;
constexpr float SettingsButtonRightEndStart = 32;
constexpr float SettingsButtonBottom = 63;

Paint HorizontalGradientByPlace(float left, const std::vector<ColorStop>& stopsByPlace)
{
    float lastPlace = stopsByPlace.back().share;

    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = {left, 0};
    paint.end = {left + lastPlace, 0};
    for (const ColorStop& stop : stopsByPlace)
        paint.stops.push_back({stop.share / lastPlace, stop.color});
    return paint;
}

void FillPolygon(VectorCanvas& canvas, float left, const std::vector<Point>& corners, const Paint& paint)
{
    VectorPath polygon;
    for (Point corner : corners)
        polygon.LineTo({left + corner.x, corner.y});
    polygon.Close();
    canvas.Fill(polygon, paint);
}

void FillRoundedBox(VectorCanvas& canvas, float left, float top, float right, float bottom, float radius, const Paint& paint)
{
    VectorPath box;
    box.AddRoundedRectangle(left, top, right - left, bottom - top, radius);
    canvas.Fill(box, paint);
}

// A dark see-through edge around a gray rim; the rim is darker on the right side.
void DrawSettingsButtonEdge(VectorCanvas& canvas, float left)
{
    Paint edge = VerticalGradientByHeight({{0, {3, 3, 3, 102}}, {SettingsButtonBottom, {27, 27, 27, 154}}});
    FillRoundedBox(canvas, left, 0, left + SettingsButtonWidth, SettingsButtonBottom, 5, edge);
    FillRoundedBox(canvas, left, 1, left + SettingsButtonWidth, 62, 4.5f, SolidPaint(Gray(82)));
    Paint darkerOnTheRight = HorizontalGradientByPlace(left, {{50, Gray(82)}, {53, Gray(37)}});
    FillRoundedBox(canvas, left + 46, 1, left + SettingsButtonWidth, 62, 4.5f, darkerOnTheRight);
}

// The body darkens a little from the left to the middle and much more towards the right side.
void DrawSettingsButtonBody(VectorCanvas& canvas, float left)
{
    Paint body = HorizontalGradientByPlace(left, {
        {2.5f, Gray(223)},
        {3.5f, Gray(225)},
        {7.5f, Gray(225)},
        {10.5f, Gray(223)},
        {14.5f, Gray(221)},
        {16.5f, Gray(220)},
        {40.5f, Gray(220)},
        {41.5f, Gray(215)},
        {43.5f, Gray(204)},
        {45.5f, Gray(187)},
        {47.5f, Gray(162)},
        {49.5f, Gray(139)},
        {51.5f, Gray(121)},
        {53.5f, Gray(113)},
    });
    FillRoundedBox(canvas, left + 2, 2, left + 54, 61, 3, body);
}

// The bottom darkens downwards in the same steps as the right side does to the right; in the corner between them the gray darkens with
// the distance from the corner of the flat part, so the bottom turns round into the side.
constexpr Point SettingsButtonFlatCorner = {40, 46};

const std::vector<ColorStop> SettingsButtonBevelByDistance = {
    {0.5f, Gray(220)},
    {2.5f, Gray(211)},
    {4.5f, Gray(198)},
    {6.5f, Gray(177)},
    {8.5f, Gray(153)},
    {10.5f, Gray(130)},
    {12.5f, Gray(115)},
    {14.5f, Gray(99)},
};

void DrawSettingsButtonBottom(VectorCanvas& canvas, float left)
{
    std::vector<ColorStop> bottomByHeight;
    for (const ColorStop& stop : SettingsButtonBevelByDistance)
        bottomByHeight.push_back({SettingsButtonFlatCorner.y + stop.share, stop.color});
    FillPolygon(canvas, left, {{2, 46}, {40, 46}, {40, 61}, {4, 61}, {2, 59}}, VerticalGradientByHeight(bottomByHeight));

    constexpr float InsideRight = 54;
    constexpr float InsideBottom = 61;
    constexpr float InsideCornerRadius = 3;
    Point corner = {left + SettingsButtonFlatCorner.x, SettingsButtonFlatCorner.y};
    Point roundCorner = {left + InsideRight - InsideCornerRadius, InsideBottom - InsideCornerRadius};

    VectorPath cornerPart;
    cornerPart.MoveTo(corner);
    cornerPart.LineTo({left + InsideRight, corner.y});
    cornerPart.ArcTo(roundCorner, InsideCornerRadius, InsideCornerRadius, 0, 90);
    cornerPart.LineTo({corner.x, InsideBottom});
    cornerPart.Close();
    canvas.Fill(cornerPart, RadialGradientByDistance(corner, SettingsButtonBevelByDistance));
}

// Along the top: a gray line under the rim, then a light band that darkens towards the right side.
void DrawSettingsButtonTop(VectorCanvas& canvas, float left)
{
    Paint band = HorizontalGradientByPlace(left, {{42, Gray(247)}, {47, Gray(225)}, {52, ClearGray(205)}});
    FillPolygon(canvas, left, {{4, 3}, {50, 3}, {53, 6}, {3, 6}}, band);
    FillPolygon(canvas, left, {{4, 2}, {50, 2}, {51, 3}, {3, 3}}, SolidPaint(Gray(135)));
}

// The left edge under the gloss: light at the top, a little darker further down.
void DrawSettingsButtonLeftStrip(VectorCanvas& canvas, float left)
{
    Paint strip = VerticalGradientByHeight({{6.5f, Gray(240)}, {19.5f, Gray(224)}});
    FillPolygon(canvas, left, {{2, 6}, {11, 6}, {5, 12.5f}, {5, 46}, {2, 46}}, strip);
}

// The white gloss: a broad band along the top with round slanted ends, and a tongue down the left side.
void DrawSettingsButtonGloss(VectorCanvas& canvas, float left)
{
    auto at = [left](float x, float y) { return Point{left + x, y}; };

    VectorPath gloss;
    gloss.MoveTo(at(10.6f, 6));
    gloss.LineTo(at(46, 6));
    gloss.LineTo(at(46.8f, 7.5f));
    gloss.CurveTo(at(46.7f, 11), at(42, 15), at(37, 17.8f));
    gloss.LineTo(at(17.5f, 17.8f));
    gloss.CurveTo(at(12, 19), at(5, 25), at(5, 31));
    gloss.LineTo(at(5, 13.2f));
    gloss.CurveTo(at(5.5f, 10), at(8, 7.2f), at(10.6f, 6));
    gloss.Close();
    canvas.Fill(gloss, SolidPaint(Gray(255)));
}

void DrawSettingsButton(VectorCanvas& canvas, float left)
{
    DrawSettingsButtonEdge(canvas, left);
    DrawSettingsButtonBody(canvas, left);
    DrawSettingsButtonBottom(canvas, left);
    DrawSettingsButtonTop(canvas, left);
    DrawSettingsButtonLeftStrip(canvas, left);
    DrawSettingsButtonGloss(canvas, left);
}

// ---- The HOME button

constexpr Point HomeButtonCenter = {28.34f, 28};
constexpr float HomeButtonRimRadius = 21.6f;
constexpr float HomeButtonDiscRadius = 20.7f;
constexpr Color HomeButtonRim = {0, 8, 123, 255};

// The disc is white in a round patch in the upper left and turns lavender towards the lower right.
constexpr Point HomeButtonLightCenter = {21, 21};
const std::vector<ColorStop> HomeButtonDiscByDistance = {
    {19, {239, 239, 247, 255}},
    {21, {231, 231, 247, 255}},
    {23, {222, 222, 239, 255}},
    {24.5f, {214, 214, 231, 255}},
    {27.5f, {206, 206, 222, 255}},
};
constexpr Point HomeButtonWhitePatchCenter = {25, 23.6f};
constexpr float HomeButtonWhitePatchRadius = 12.3f;
constexpr float HomeButtonWhitePatchSoftEdge = 1.5f;
constexpr Color White = {255, 255, 255, 255};
constexpr Color ClearWhite = {255, 255, 255, 0};

// The house: a roof with eaves wider than the walls, a white window, and its right part and its foot a little darker.
constexpr Color HouseBlue = {148, 189, 255, 255};
constexpr Color HouseShadeBlue = {140, 181, 247, 255};
constexpr Color HouseEdgeBlue = {140, 165, 214, 160};
constexpr Color HouseLineUnderTheRoof = {173, 206, 255, 255};
constexpr Color WindowFrame = {189, 202, 231, 255};
constexpr Color WindowSill = {132, 173, 231, 160};
constexpr Color WindowWhite = {239, 239, 247, 255};
constexpr Point RoofTop = {28.4f, 15.6f};
constexpr float EavesY = 29.1f;
constexpr float LeftEaveX = 14.4f;
constexpr float RightEaveX = 42.4f;
constexpr float WallLeft = 18.7f;
constexpr float WallRight = 37;
constexpr float HouseBottom = 39;
constexpr float WindowLeft = 23.6f;
constexpr float WindowRight = 31.6f;
constexpr float WindowBottom = 35;
constexpr float EdgeLineWidth = 1;

void FillPolygonAt(VectorCanvas& canvas, const std::vector<Point>& corners, const Paint& paint)
{
    VectorPath polygon;
    for (Point corner : corners)
        polygon.LineTo(corner);
    polygon.Close();
    canvas.Fill(polygon, paint);
}

void DrawStroke(VectorCanvas& canvas, Point from, Point to, Color color)
{
    VectorPath stroke;
    stroke.AddStroke(from, to, EdgeLineWidth);
    canvas.Fill(stroke, SolidPaint(color));
}

void DrawHouse(VectorCanvas& canvas)
{
    FillPolygonAt(canvas, {
        RoofTop,
        {RightEaveX, EavesY},
        {WallRight, EavesY},
        {WallRight, HouseBottom},
        {WallLeft, HouseBottom},
        {WallLeft, EavesY},
        {LeftEaveX, EavesY},
    }, SolidPaint(HouseBlue));

    FillPolygonAt(canvas, {{32, 27.5f}, {35.3f, 23.6f}, {40.9f, 29}, {WallRight, 29}, {WallRight, HouseBottom}, {32, HouseBottom}},
        SolidPaint(HouseShadeBlue));
    FillRectangle(canvas, WallLeft, 35.5f, WallRight, HouseBottom, SolidPaint(HouseShadeBlue));
    FillRectangle(canvas, 15.5f, 28, WindowRight, 29, SolidPaint(HouseLineUnderTheRoof));

    DrawStroke(canvas, {28.4f, 16.4f}, {15.5f, 28.6f}, HouseEdgeBlue);
    DrawStroke(canvas, {19.2f, 29.5f}, {19.2f, 38.5f}, HouseEdgeBlue);
    DrawStroke(canvas, {32.4f, 29.5f}, {32.4f, 35.6f}, HouseEdgeBlue);
}

void DrawWindow(VectorCanvas& canvas)
{
    FillRectangle(canvas, WindowLeft, EavesY, WindowRight, WindowBottom, LinearGradient({24, 29}, White, {30, 34}, WindowWhite));
    FillRectangle(canvas, WindowLeft, WindowBottom, WindowRight, WindowBottom + 1, SolidPaint(WindowFrame));
    FillRectangle(canvas, WindowLeft, WindowBottom + 1, 32, WindowBottom + 2, SolidPaint(WindowSill));
}

}

void DrawBigButtonLeftEnd(VectorCanvas& canvas)
{
    DrawBigButtonEndBody(canvas, LeftEndCenterX, ShadeCenterOfLeftEnd(), LeftEndBandByDegrees);
    DrawLeftEndHighlight(canvas);
}

void DrawBigButtonMiddle(VectorCanvas& canvas)
{
    DrawBigButtonMiddleColumn(canvas);
}

void DrawBigButtonRightEnd(VectorCanvas& canvas)
{
    DrawBigButtonEndBody(canvas, RightEndCenterX, ShadeCenterOfRightEnd(), RightEndBandByDegrees);
    DrawRightEndHighlight(canvas);
}

void DrawSettingsButtonLeftEnd(VectorCanvas& canvas)
{
    DrawSettingsButton(canvas, 0);
}

void DrawSettingsButtonMiddle(VectorCanvas& canvas)
{
    DrawSettingsButton(canvas, -SettingsButtonMiddleStart);
}

void DrawSettingsButtonRightEnd(VectorCanvas& canvas)
{
    DrawSettingsButton(canvas, -SettingsButtonRightEndStart);
}

void DrawHomeButtonPicture(VectorCanvas& canvas)
{
    FillCircle(canvas, HomeButtonCenter, HomeButtonRimRadius, SolidPaint(HomeButtonRim));
    FillCircle(canvas, HomeButtonCenter, HomeButtonDiscRadius, RadialGradientByDistance(HomeButtonLightCenter, HomeButtonDiscByDistance));

    float softEdgeStart = HomeButtonWhitePatchRadius - HomeButtonWhitePatchSoftEdge;
    Paint whitePatch = RadialGradient(HomeButtonWhitePatchCenter, White, HomeButtonWhitePatchRadius, ClearWhite, softEdgeStart);
    FillCircle(canvas, HomeButtonWhitePatchCenter, HomeButtonWhitePatchRadius, whitePatch);

    DrawHouse(canvas);
    DrawWindow(canvas);
}

void DrawBatteryCase(VectorCanvas& canvas)
{
    VectorPath battery;
    battery.AddPolygon({{6, 3}, {40, 3}, {40, 7}, {43, 7}, {43, 17}, {40, 17}, {40, 21}, {6, 21}});
    battery.AddPolygon({{8, 5}, {38, 5}, {38, 9}, {41, 9}, {41, 15}, {38, 15}, {38, 19}, {8, 19}});
    canvas.Fill(battery, SolidPaint(White));
}

void DrawBatteryBar(VectorCanvas& canvas)
{
    FillRectangle(canvas, 5, 3, 10, 13, SolidPaint(White));
}

constexpr float SignMiddle = 12;
constexpr float SignHalfThickness = 1.6f;
constexpr float SignLength = 24;

void DrawMinusSign(VectorCanvas& canvas)
{
    FillRectangle(canvas, 0, SignMiddle - SignHalfThickness, SignLength, SignMiddle + SignHalfThickness, SolidPaint(White));
}

void DrawPlusSign(VectorCanvas& canvas)
{
    VectorPath plus;
    plus.AddPolygon({
        {SignMiddle - SignHalfThickness, 0},
        {SignMiddle + SignHalfThickness, 0},
        {SignMiddle + SignHalfThickness, SignMiddle - SignHalfThickness},
        {SignLength, SignMiddle - SignHalfThickness},
        {SignLength, SignMiddle + SignHalfThickness},
        {SignMiddle + SignHalfThickness, SignMiddle + SignHalfThickness},
        {SignMiddle + SignHalfThickness, SignLength},
        {SignMiddle - SignHalfThickness, SignLength},
        {SignMiddle - SignHalfThickness, SignMiddle + SignHalfThickness},
        {0, SignMiddle + SignHalfThickness},
        {0, SignMiddle - SignHalfThickness},
        {SignMiddle - SignHalfThickness, SignMiddle - SignHalfThickness},
    });
    canvas.Fill(plus, SolidPaint(White));
}

void DrawVolumeStep(VectorCanvas& canvas)
{
    FillRectangle(canvas, 0, 0, 8, 16, SolidPaint(White));
}

void DrawBlinkingDisc(VectorCanvas& canvas)
{
    constexpr Point Center = {12, 12};
    constexpr float EdgeRadius = 10.41f;
    constexpr float WhiteRadius = 9.43f;
    constexpr Color Black = {0, 0, 0, 255};

    FillCircle(canvas, Center, EdgeRadius, SolidPaint(Black));
    FillCircle(canvas, Center, WhiteRadius, SolidPaint(White));
}

void DrawWindowCorner(VectorCanvas& canvas)
{
    constexpr float CornerSize = 16;
    constexpr float RoundRadius = 12;
    constexpr float EdgeWidth = 1;
    constexpr float EdgeOverlap = 2;
    constexpr Color Black = {0, 0, 0, 255};

    // The rounded rectangles reach beyond the picture on the right and at the bottom, so only their upper left corner shows.
    float beyond = CornerSize + RoundRadius + EdgeOverlap;
    VectorPath edge;
    edge.AddRoundedRectangle(0, 0, beyond, beyond, RoundRadius);
    canvas.Fill(edge, SolidPaint(White));

    VectorPath inside;
    inside.AddRoundedRectangle(EdgeWidth, EdgeWidth, beyond, beyond, RoundRadius - EdgeWidth);
    canvas.Fill(inside, SolidPaint(Black));
}

}
