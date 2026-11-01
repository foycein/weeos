#include "SharpArt/WiiLogoPictures.h"

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};

// The places of the word are those of the 72 by 33 picture of the logo of Wii Options and Wii Settings; a placement moves and scales
// them to where another picture shows the word.
struct Placement
{
    Point offset = {0, 0};
    float scale = 1;

    Point Place(Point pointOfTheLogo) const
    {
        return {offset.x + pointOfTheLogo.x * scale, offset.y + pointOfTheLogo.y * scale};
    }
};

// The W: four slanted strokes that meet in two round valleys at the bottom and a round peak at the top. The outline goes around
// clockwise from the top left corner.
void AddLetterW(VectorPath& path, const Placement& placement)
{
    constexpr float TopY = 5;
    constexpr float BottomOfStrokesY = 28.5f;
    constexpr float CurveControlBelowValleys = 31;
    constexpr float CurveControlAbovePeak = 4.1f;

    auto place = [&placement](Point point) { return placement.Place(point); };

    path.MoveTo(place({6.2f, TopY}));
    path.LineTo(place({12.05f, TopY}));
    path.LineTo(place({16.6f, 22}));
    path.LineTo(place({22.45f, TopY}));
    path.CurveTo(place({23.6f, CurveControlAbovePeak}), place({25.6f, CurveControlAbovePeak}), place({26.75f, TopY}));
    path.LineTo(place({32.35f, 23.3f}));
    path.LineTo(place({37, TopY}));
    path.LineTo(place({43, TopY}));
    path.LineTo(place({36.25f, BottomOfStrokesY}));
    path.CurveTo(place({35.6f, CurveControlBelowValleys}), place({29.7f, CurveControlBelowValleys}), place({29.05f, BottomOfStrokesY}));
    path.LineTo(place({24.6f, 13.2f}));
    path.LineTo(place({20.15f, BottomOfStrokesY}));
    path.CurveTo(place({19.5f, CurveControlBelowValleys}), place({13.25f, CurveControlBelowValleys}), place({12.6f, BottomOfStrokesY}));
    path.Close();
}

void AddLetterI(VectorPath& path, float middleX, const Placement& placement)
{
    constexpr float StemHalfWidth = 2.63f;
    constexpr float StemTop = 12.87f;
    constexpr float StemBottom = 30.33f;
    constexpr float DotY = 6.4f;
    constexpr float DotRadius = 3.2f;

    Point stemTopLeft = placement.Place({middleX - StemHalfWidth, StemTop});
    Point stemBottomRight = placement.Place({middleX + StemHalfWidth, StemBottom});
    path.AddRectangle(stemTopLeft.x, stemTopLeft.y, stemBottomRight.x - stemTopLeft.x, stemBottomRight.y - stemTopLeft.y);
    path.AddCircle(placement.Place({middleX, DotY}), DotRadius * placement.scale);
}

}

void AddWiiWord(VectorPath& path, Point offset, float scale)
{
    constexpr float FirstIX = 48.5f;
    constexpr float SecondIX = 59.5f;

    Placement placement{offset, scale};
    AddLetterW(path, placement);
    AddLetterI(path, FirstIX, placement);
    AddLetterI(path, SecondIX, placement);
}

void DrawWiiLogoOfTheSettings(VectorCanvas& canvas)
{
    VectorPath logo;
    AddWiiWord(logo, {0, 0}, 1);
    canvas.Fill(logo, SolidPaint(White));
}

}
