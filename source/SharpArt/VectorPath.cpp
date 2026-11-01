#include "SharpArt/VectorPath.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace SharpArt
{

namespace
{

// Enough short lines that a curve looks round at the largest size a picture is drawn at.
constexpr int LinesPerCurve = 48;
constexpr int LinesPerFullCircle = 192;
constexpr float DegreesPerHalfTurn = 180;
constexpr float FullTurnDegrees = 360;
constexpr float QuarterTurnDegrees = 90;

Point PointOnCurve(Point start, Point firstControl, Point secondControl, Point end, float share)
{
    float rest = 1 - share;
    float startWeight = rest * rest * rest;
    float firstWeight = 3 * rest * rest * share;
    float secondWeight = 3 * rest * share * share;
    float endWeight = share * share * share;

    Point point;
    point.x = startWeight * start.x + firstWeight * firstControl.x + secondWeight * secondControl.x + endWeight * end.x;
    point.y = startWeight * start.y + firstWeight * firstControl.y + secondWeight * secondControl.y + endWeight * end.y;
    return point;
}

Point MoveTowards(Point from, Point to, float distance)
{
    float length = std::hypot(to.x - from.x, to.y - from.y);
    if (length == 0)
        return from;

    float share = distance / length;
    return {from.x + (to.x - from.x) * share, from.y + (to.y - from.y) * share};
}

// A cubic curve that follows a quarter circle best has its control points this far from its ends, as a share of the radius.
constexpr float CircleControlShare = 0.55f;

Point PointOnEllipse(Point center, float radiusX, float radiusY, float degrees)
{
    float radians = degrees * std::numbers::pi_v<float> / DegreesPerHalfTurn;
    return {center.x + radiusX * std::cos(radians), center.y + radiusY * std::sin(radians)};
}

}

void VectorPath::MoveTo(Point point)
{
    outlines.push_back({point});
    isOutlineOpen = true;
}

void VectorPath::LineTo(Point point)
{
    if (!isOutlineOpen)
    {
        MoveTo(point);
        return;
    }
    outlines.back().push_back(point);
}

void VectorPath::CurveTo(Point firstControl, Point secondControl, Point end)
{
    if (!isOutlineOpen)
        MoveTo(firstControl);

    Outline& outline = outlines.back();
    Point start = outline.back();

    for (int step = 1; step <= LinesPerCurve; ++step)
    {
        float share = static_cast<float>(step) / LinesPerCurve;
        outline.push_back(PointOnCurve(start, firstControl, secondControl, end, share));
    }
}

void VectorPath::ArcTo(Point center, float radiusX, float radiusY, float startDegrees, float endDegrees)
{
    Point start = PointOnEllipse(center, radiusX, radiusY, startDegrees);
    if (isOutlineOpen)
        LineTo(start);
    else
        MoveTo(start);

    float sweep = endDegrees - startDegrees;
    int stepCount = static_cast<int>(std::ceil(std::abs(sweep) / FullTurnDegrees * LinesPerFullCircle));
    for (int step = 1; step <= stepCount; ++step)
    {
        float degrees = startDegrees + sweep * static_cast<float>(step) / static_cast<float>(stepCount);
        LineTo(PointOnEllipse(center, radiusX, radiusY, degrees));
    }
}

void VectorPath::Close()
{
    isOutlineOpen = false;
}

void VectorPath::AddPolygon(std::initializer_list<Point> corners)
{
    Close();
    outlines.emplace_back(corners);
}

void VectorPath::AddPolygonWithRoundCorners(const std::vector<Point>& corners, float cornerCut)
{
    Close();
    std::size_t count = corners.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        Point previous = corners[(index + count - 1) % count];
        Point corner = corners[index];
        Point next = corners[(index + 1) % count];

        float previousSide = std::hypot(previous.x - corner.x, previous.y - corner.y);
        float nextSide = std::hypot(next.x - corner.x, next.y - corner.y);
        float cut = std::min({cornerCut, previousSide / 2, nextSide / 2});

        Point curveStart = MoveTowards(corner, previous, cut);
        Point curveEnd = MoveTowards(corner, next, cut);
        LineTo(curveStart);
        CurveTo(MoveTowards(curveStart, corner, cut * CircleControlShare), MoveTowards(curveEnd, corner, cut * CircleControlShare), curveEnd);
    }
    Close();
}

void VectorPath::AddRectangle(float left, float top, float width, float height)
{
    float right = left + width;
    float bottom = top + height;
    AddPolygon({{left, top}, {right, top}, {right, bottom}, {left, bottom}});
}

void VectorPath::AddRoundedRectangle(float left, float top, float width, float height, float cornerRadius)
{
    float right = left + width;
    float bottom = top + height;
    float radius = cornerRadius;

    Close();
    ArcTo({right - radius, top + radius}, radius, radius, -QuarterTurnDegrees, 0);
    ArcTo({right - radius, bottom - radius}, radius, radius, 0, QuarterTurnDegrees);
    ArcTo({left + radius, bottom - radius}, radius, radius, QuarterTurnDegrees, 2 * QuarterTurnDegrees);
    ArcTo({left + radius, top + radius}, radius, radius, 2 * QuarterTurnDegrees, 3 * QuarterTurnDegrees);
    Close();
}

void VectorPath::AddEllipse(Point center, float radiusX, float radiusY)
{
    Close();
    ArcTo(center, radiusX, radiusY, 0, FullTurnDegrees);
    Close();
}

void VectorPath::AddCircle(Point center, float radius)
{
    AddEllipse(center, radius, radius);
}

void VectorPath::AddStroke(Point from, Point to, float width)
{
    float radius = width / 2;
    float directionDegrees = std::atan2(to.y - from.y, to.x - from.x) * DegreesPerHalfTurn / std::numbers::pi_v<float>;

    Close();
    ArcTo(to, radius, radius, directionDegrees - QuarterTurnDegrees, directionDegrees + QuarterTurnDegrees);
    ArcTo(from, radius, radius, directionDegrees + QuarterTurnDegrees, directionDegrees + 3 * QuarterTurnDegrees);
    Close();
}

const std::vector<Outline>& VectorPath::Outlines() const
{
    return outlines;
}

}
