#include "SharpArt/InternetChannelPictures.h"

#include <initializer_list>
#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};

// Directions in degrees: 0 to the right, 90 down, 180 to the left, 270 up; the left side again is -180.
constexpr float LeftDegrees = 180;
constexpr float UpDegrees = 270;
constexpr float RightDegrees = 360;
constexpr float LowerRightDegrees = 45;
constexpr float LeftDegreesGoingBack = -180;

// The arrows point left and right; the right one is the left one seen in a mirror.
constexpr float MirrorX = 64;

void FillWhite(VectorCanvas& canvas, const VectorPath& path)
{
    canvas.Fill(path, SolidPaint(White));
}

void FillRoundPolygon(VectorCanvas& canvas, const std::vector<RoundCorner>& corners)
{
    VectorPath polygon;
    polygon.AddPolygonWithRoundCorners(corners);
    FillWhite(canvas, polygon);
}

void FillPolygon(VectorCanvas& canvas, std::initializer_list<Point> corners)
{
    VectorPath polygon;
    polygon.AddPolygon(corners);
    FillWhite(canvas, polygon);
}

// A part of a thick ring between two directions, going clockwise from the first one.
void FillRingPart(VectorCanvas& canvas, Point center, float outerRadius, float innerRadius, float fromDegrees, float toDegrees)
{
    VectorPath ringPart;
    ringPart.ArcTo(center, outerRadius, outerRadius, fromDegrees, toDegrees);
    ringPart.ArcTo(center, innerRadius, innerRadius, toDegrees, fromDegrees);
    ringPart.Close();
    FillWhite(canvas, ringPart);
}

void FillCircle(VectorCanvas& canvas, Point center, float radius)
{
    VectorPath circle;
    circle.AddCircle(center, radius);
    FillWhite(canvas, circle);
}

std::vector<RoundCorner> Mirrored(const std::vector<RoundCorner>& corners)
{
    std::vector<RoundCorner> mirrored;
    for (const RoundCorner& corner : corners)
        mirrored.push_back({{MirrorX - corner.place.x, corner.place.y}, corner.cut});
    return mirrored;
}

// An oval made of four curves. With a curve share of 0.55 it is an ellipse; a larger share makes it more like a rectangle with round corners.
void AddSquarishEllipse(VectorPath& path, Point center, float radiusX, float radiusY, float curveShare)
{
    float handleX = radiusX * curveShare;
    float handleY = radiusY * curveShare;
    Point top = {center.x, center.y - radiusY};
    Point right = {center.x + radiusX, center.y};
    Point bottom = {center.x, center.y + radiusY};
    Point left = {center.x - radiusX, center.y};

    path.Close();
    path.MoveTo(top);
    path.CurveTo({top.x + handleX, top.y}, {right.x, right.y - handleY}, right);
    path.CurveTo({right.x, right.y + handleY}, {bottom.x + handleX, bottom.y}, bottom);
    path.CurveTo({bottom.x - handleX, bottom.y}, {left.x, left.y + handleY}, left);
    path.CurveTo({left.x, left.y - handleY}, {top.x - handleX, top.y}, top);
    path.Close();
}

// The places of the question mark are those of the big one (64 by 64); a placement moves and scales it into the house.
struct Placement
{
    Point offset = {0, 0};
    float scale = 1;

    Point Place(Point point) const
    {
        return {offset.x + point.x * scale, offset.y + point.y * scale};
    }

    float Size(float size) const
    {
        return size * scale;
    }
};

// The hook of the question mark: a thick bow over the top, whose left end is cut off level and whose right end bends down into the
// stem; then the dot below.
void AddQuestionMark(VectorPath& path, const Placement& placement)
{
    constexpr Point BowCenter = {32.1f, 21.7f};
    constexpr float BowRadiusX = 21.9f;
    constexpr float BowRadiusY = 20.2f;
    constexpr float BowTop = BowCenter.y - BowRadiusY;
    constexpr Point HoleCenter = {31.4f, 19.4f};
    constexpr float HoleRadius = 5.7f;
    constexpr float LeftEndBottomInside = 22.4f;
    constexpr float LeftEndBottomOutside = 20.6f;
    constexpr float StemBottom = 43.6f;
    constexpr Point DotCenter = {30.5f, 54.6f};
    constexpr float DotRadius = 8.15f;

    auto place = [&placement](Point point) { return placement.Place(point); };

    path.ArcTo(place(BowCenter), placement.Size(BowRadiusX), placement.Size(BowRadiusY), LeftDegrees, UpDegrees);
    path.CurveTo(place({44.5f, BowTop}), place({53.4f, 8.5f}), place({53.4f, 20}));
    path.CurveTo(place({53.4f, 27}), place({38.3f, 33}), place({37.8f, StemBottom}));
    path.LineTo(place({23.4f, StemBottom}));
    path.CurveTo(place({22.5f, 33}), place({28, 27}), place({35.43f, 23.43f}));
    path.ArcTo(place(HoleCenter), placement.Size(HoleRadius), placement.Size(HoleRadius), LowerRightDegrees, LeftDegreesGoingBack);
    path.LineTo(place({HoleCenter.x - HoleRadius, LeftEndBottomInside}));
    path.LineTo(place({10.6f, LeftEndBottomOutside}));
    path.Close();

    path.AddCircle(place(DotCenter), placement.Size(DotRadius));
}

const std::vector<RoundCorner> ArrowLeftCorners = {
    {{-2.65f, 32}, 8},
    {{44.2f, 4.6f}, 7},
    {{44.2f, 21.6f}, 0.5f},
    {{62.2f, 21.6f}, 1.5f},
    {{62.2f, 41.6f}, 1.5f},
    {{44.2f, 41.6f}, 0.5f},
    {{44.2f, 59.4f}, 7},
};

}

void DrawInternetBall(VectorCanvas& canvas)
{
    FillCircle(canvas, {28.33f, 28.3f}, 24.2f);
}

void DrawInternetBar(VectorCanvas& canvas)
{
    VectorPath bar;
    bar.AddRoundedRectangle(3.67f, 3.4f, 49, 120, 4.5f);
    FillWhite(canvas, bar);
}

// Two barrels that widen downwards into round ends, with a notch between them at the top and at the bottom, and the oval lenses.
void DrawInternetBinoculars(VectorCanvas& canvas)
{
    constexpr float BarrelEndRadius = 14.2f;
    constexpr float BarrelEndY = 43.6f;
    constexpr float LeftBarrelX = 16;
    constexpr float RightBarrelX = 48;
    constexpr float LensY = 47.4f;
    constexpr float LensRadiusX = 7.55f;
    constexpr float LensRadiusY = 3.8f;

    constexpr float NotchBottomY = 45.5f;
    FillRoundPolygon(canvas, {{{14.3f, 7.4f}, 7}, {{27.05f, 7.4f}, 4}, {{32, 12.7f}, 0.5f}, {{36.95f, 7.4f}, 4}, {{49.7f, 7.4f}, 7},
                                 {{61.8f, 40.5f}, 0}, {{RightBarrelX, NotchBottomY}, 0}, {{LeftBarrelX, NotchBottomY}, 0}, {{2.2f, 40.5f}, 0}});
    FillCircle(canvas, {LeftBarrelX, BarrelEndY}, BarrelEndRadius);
    FillCircle(canvas, {RightBarrelX, BarrelEndY}, BarrelEndRadius);

    VectorPath lenses;
    lenses.AddEllipse({16.75f, LensY}, LensRadiusX, LensRadiusY);
    lenses.AddEllipse({47.25f, LensY}, LensRadiusX, LensRadiusY);
    canvas.Cut(lenses);
}

void DrawInternetHouseWithQuestionMark(VectorCanvas& canvas)
{
    constexpr float EavesY = 37.4f;
    constexpr float WallLeft = 10.4f;
    constexpr float WallRight = 53.6f;
    constexpr float WallBottom = 59.6f;
    constexpr Placement QuestionMarkInTheHouse = {{15.1f, 19.5f}, 0.535f};

    FillRoundPolygon(canvas, {{{32, 5.3f}, 4}, {{64.7f, EavesY}, 4}, {{WallRight, EavesY}, 0}, {{WallRight, WallBottom}, 5},
                                 {{WallLeft, WallBottom}, 5}, {{WallLeft, EavesY}, 0}, {{-0.7f, EavesY}, 4}});

    VectorPath questionMark;
    AddQuestionMark(questionMark, QuestionMarkInTheHouse);
    canvas.Cut(questionMark);
}

void DrawInternetArrowLeft(VectorCanvas& canvas)
{
    FillRoundPolygon(canvas, ArrowLeftCorners);
}

void DrawInternetStar(VectorCanvas& canvas)
{
    constexpr float ArmTopY = 21.6f;
    constexpr float TipCut = 6;
    constexpr float ArmTipCut = 3;
    constexpr float InnerCut = 1;

    FillRoundPolygon(canvas, {{{31.8f, -0.2f}, TipCut}, {{42.9f, ArmTopY}, InnerCut}, {{64.54f, 24.4f}, ArmTipCut},
                                 {{48.7f, 39.9f}, InnerCut}, {{52.1f, 62.4f}, TipCut}, {{31.85f, 52.2f}, InnerCut}, {{11.5f, 62.4f}, TipCut},
                                 {{14.9f, 39.9f}, InnerCut}, {{-0.94f, 24.4f}, ArmTipCut}, {{20.7f, ArmTopY}, InnerCut}});
}

// A white disc with the letter i cut out of it.
void DrawInternetInformation(VectorCanvas& canvas)
{
    constexpr float StemLeft = 26.8f;
    constexpr float StemRight = 37.2f;
    constexpr float FootLeft = 22.6f;
    constexpr float FootRight = 41.4f;
    constexpr float FootTop = 54;
    constexpr float FootBottom = 55.9f;

    FillCircle(canvas, {32.1f, 32.4f}, 30.7f);

    VectorPath letter;
    letter.AddCircle({31.9f, 14.4f}, 5.9f);
    letter.AddPolygonWithRoundCorners({{{23.1f, 25.8f}, 0.5f}, {{33, 25}, 1}, {{StemRight, 23.6f}, 0.5f}, {{StemRight, 50.6f}, 1.5f},
        {{FootRight, FootTop}, 0.8f}, {{FootRight, FootBottom}, 0.3f}, {{FootLeft, FootBottom}, 0.3f}, {{FootLeft, FootTop}, 0.8f},
        {{StemLeft, 50.6f}, 1.5f}, {{StemLeft, 29.6f}, 0}, {{23.1f, 29.6f}, 0.3f}});
    canvas.Cut(letter);
}

// A tall ring and, over its lower right part, a house with a door; a clear margin runs around the house.
void DrawInternetRingWithHouse(VectorCanvas& canvas)
{
    constexpr Point RingCenter = {25.6f, 29.9f};
    constexpr Point HoleCenter = {25.6f, 29.4f};
    constexpr float HouseMargin = 2.7f;
    constexpr float EavesY = 45.6f;
    constexpr float WallLeft = 28.2f;
    constexpr float WallRight = 56.8f;
    constexpr float WallBottom = 61.4f;

    VectorPath ring;
    AddSquarishEllipse(ring, RingCenter, 23.2f, 28.1f, 0.62f);
    AddSquarishEllipse(ring, HoleCenter, 10.6f, 25.6f, 0.72f);
    FillWhite(canvas, ring);

    VectorPath house;
    house.AddPolygonWithRoundCorners({{{42.5f, 25.1f}, 1}, {{63, EavesY}, 1}, {{WallRight, EavesY}, 0}, {{WallRight, WallBottom}, 0.5f},
        {{WallLeft, WallBottom}, 0.5f}, {{WallLeft, EavesY}, 0}, {{22, EavesY}, 1}});
    canvas.Cut(house);
    const Outline& outline = house.Outlines().front();
    for (std::size_t index = 0; index < outline.size(); ++index)
    {
        VectorPath margin;
        margin.AddStroke(outline[index], outline[(index + 1) % outline.size()], 2 * HouseMargin);
        canvas.Cut(margin);
    }
    FillWhite(canvas, house);

    VectorPath door;
    door.AddRectangle(37.6f, 47.5f, 10.2f, 10);
    canvas.Cut(door);
}

void DrawInternetArrowRight(VectorCanvas& canvas)
{
    FillRoundPolygon(canvas, Mirrored(ArrowLeftCorners));
}

// Two arrows that chase each other around a circle: the upper one ends pointing down on the right, the lower one pointing up on the left.
// Each arrow is a crescent: the circle of the outside minus a smaller circle moved towards the arrow's tail, so the arrow is thick at
// its head and runs out thin at its tail.
void DrawInternetTurningArrows(VectorCanvas& canvas)
{
    constexpr Point OuterCenter = {32, 32.67f};
    constexpr float OuterRadius = 22.8f;
    constexpr Point UpperInnerCenter = {27.4f, 32.8f};
    constexpr float UpperInnerRadius = 17.1f;
    constexpr Point LowerInnerCenter = {36.3f, 32.2f};
    constexpr float LowerInnerRadius = 17.3f;
    constexpr float UpperTailDegrees = 223;
    constexpr float LowerTailDegrees = 43;
    constexpr float HeadCornerCut = 1;

    VectorPath upperArrow;
    upperArrow.ArcTo(OuterCenter, OuterRadius, OuterRadius, UpperTailDegrees, RightDegrees);
    upperArrow.ArcTo(UpperInnerCenter, UpperInnerRadius, UpperInnerRadius, RightDegrees, UpperTailDegrees);
    upperArrow.Close();
    FillWhite(canvas, upperArrow);
    FillRoundPolygon(canvas, {{{38.2f, 31.6f}, HeadCornerCut}, {{62.4f, 31.6f}, HeadCornerCut}, {{62.4f, 33.8f}, HeadCornerCut},
                                 {{50.2f, 46}, HeadCornerCut}, {{38.2f, 34}, HeadCornerCut}});

    VectorPath lowerArrow;
    lowerArrow.ArcTo(OuterCenter, OuterRadius, OuterRadius, LowerTailDegrees, LeftDegrees);
    lowerArrow.ArcTo(LowerInnerCenter, LowerInnerRadius, LowerInnerRadius, LeftDegrees, LowerTailDegrees);
    lowerArrow.Close();
    FillWhite(canvas, lowerArrow);
    FillRoundPolygon(canvas, {{{25.8f, 33.6f}, HeadCornerCut}, {{1.4f, 33.6f}, HeadCornerCut}, {{1.4f, 31.4f}, HeadCornerCut},
                                 {{13.6f, 19.2f}, HeadCornerCut}, {{25.8f, 31.4f}, HeadCornerCut}});
}

// A wrench lying from the lower left to the upper right: a round handle end with a hole, and a round head with its jaw open to the
// upper right.
void DrawInternetWrench(VectorCanvas& canvas)
{
    constexpr Point HeadCenter = {45.1f, 20.6f};
    constexpr float HeadRadiusX = 16.7f;
    constexpr float HeadRadiusY = 15.2f;
    constexpr Point HandleEnd = {9.5f, 54.5f};
    constexpr float HandleWidth = 14.7f;

    VectorPath head;
    head.AddEllipse(HeadCenter, HeadRadiusX, HeadRadiusY);
    FillWhite(canvas, head);

    VectorPath handle;
    handle.AddStroke({40, 24}, HandleEnd, HandleWidth);
    FillWhite(canvas, handle);

    VectorPath jaw;
    jaw.AddPolygonWithRoundCorners({{{47.8f, -1}, 0}, {{47.8f, 1.7f}, 0}, {{40.4f, 11}, 0}, {{41.5f, 14.5f}, 2}, {{43.2f, 19}, 2},
        {{46, 21.3f}, 2}, {{50.5f, 23.3f}, 1.5f}, {{53.3f, 23.5f}, 1.5f}, {{65, 11.8f}, 0}, {{65, -1}, 0}});
    canvas.Cut(jaw);

    VectorPath upperJaw;
    upperJaw.AddPolygonWithRoundCorners({{{40, 18}, 0}, {{29.3f, 16.5f}, 0}, {{29.7f, 12.5f}, 2}, {{32, 8.5f}, 2}, {{36.6f, 4.5f}, 2}, {{42.2f, 1.7f}, 1}, {{47.8f, 1.7f}, 0.5f}, {{40.4f, 11}, 0}});
    FillWhite(canvas, upperJaw);

    VectorPath hole;
    hole.AddCircle(HandleEnd, 2.7f);
    canvas.Cut(hole);
}

void DrawInternetQuestionMark(VectorCanvas& canvas)
{
    VectorPath questionMark;
    AddQuestionMark(questionMark, {});
    FillWhite(canvas, questionMark);
}

// The shackle is a thick bow whose legs reach down into the body.
void DrawInternetLock(VectorCanvas& canvas)
{
    constexpr float ShackleBottom = 28.5f;

    VectorPath shackle;
    shackle.ArcTo({31.6f, 16.2f}, 14.2f, 14.2f, LeftDegrees, RightDegrees);
    shackle.LineTo({45.8f, ShackleBottom});
    shackle.LineTo({40, ShackleBottom});
    shackle.ArcTo({31.8f, 16.2f}, 8.2f, 8.2f, RightDegrees, LeftDegrees);
    shackle.LineTo({23.6f, ShackleBottom});
    shackle.LineTo({17.4f, ShackleBottom});
    shackle.Close();
    FillWhite(canvas, shackle);

    VectorPath body;
    body.AddRoundedRectangle(12, 27.6f, 39.6f, 34.1f, 2.5f);
    FillWhite(canvas, body);
}

}
