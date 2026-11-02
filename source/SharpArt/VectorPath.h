#pragma once

#include "Formats/Vectors.h"

#include <functional>
#include <initializer_list>
#include <vector>

namespace SharpArt
{

using Point = Formats::Vector2;

// One closed outline: the corners of a polygon, in order. Curves are already cut into short straight lines.
using Outline = std::vector<Point>;

// A corner of a polygon and how far along both of its sides it is cut to make it round.
struct RoundCorner
{
    Point place;
    float cut = 0;
};

// The outline of a shape, made of straight lines and curves. The units are the pixels of the original picture: x grows to the
// right, y grows downward and (0, 0) is the top left corner.
// A shape is filled where a line from the point to far away crosses the outlines an odd number of times, so an outline inside
// another one cuts a hole into it, whichever way it goes around.
class VectorPath
{
public:
    // Starts a new outline at the point.
    void MoveTo(Point point);

    // A straight line from the current point. Without a current point the line starts a new outline at the point.
    void LineTo(Point point);

    // A cubic Bezier curve from the current point to the end point. Without a current point it starts at the first control point.
    void CurveTo(Point firstControl, Point secondControl, Point end);

    // An arc of the ellipse around the center, from the start angle to the end angle, in degrees. 0 degrees points to the right
    // and 90 degrees points down. The line goes from the current point to the start of the arc; without a current point the
    // arc starts a new outline.
    void ArcTo(Point center, float radiusX, float radiusY, float startDegrees, float endDegrees);

    void Close();

    void AddPolygon(std::initializer_list<Point> corners);

    // A polygon whose corners are rounded: each corner is cut this far along both of its sides, at most half way, and the cut is a curve.
    void AddPolygonWithRoundCorners(const std::vector<Point>& corners, float cornerCut);

    // The same with a cut of its own for every corner; a cut of 0 leaves the corner sharp.
    void AddPolygonWithRoundCorners(const std::vector<RoundCorner>& corners);
    void AddRectangle(float left, float top, float width, float height);
    void AddRoundedRectangle(float left, float top, float width, float height, float cornerRadius);
    void AddEllipse(Point center, float radiusX, float radiusY);
    void AddCircle(Point center, float radius);

    // A straight stroke of the width from one point to the other, with round ends, as a pen draws it.
    void AddStroke(Point from, Point to, float width);

    // Adds the outlines of the other path to this one.
    void AddPath(const VectorPath& other);

    // Moves every point of the path to where the placement puts it, for example onto a card seen in perspective.
    void PlaceEveryPoint(const std::function<Point(Point)>& placement);

    // The closed outlines of the path. An outline that was not closed is closed here.
    const std::vector<Outline>& Outlines() const;

private:
    std::vector<Outline> outlines;
    bool isOutlineOpen = false;
};

}
