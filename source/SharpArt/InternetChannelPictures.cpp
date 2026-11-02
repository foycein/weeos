#include "SharpArt/InternetChannelPictures.h"

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

void DrawInternetArrowRight(VectorCanvas& canvas)
{
    FillRoundPolygon(canvas, Mirrored(ArrowLeftCorners));
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
