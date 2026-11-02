#pragma once

#include "SharpArt/VectorPath.h"

#include "Formats/RgbaImage.h"

#include <vector>

namespace SharpArt
{

using Color = Formats::Rgba8Color;

enum class PaintKind
{
    // One color everywhere.
    Solid,

    // Changes along the line from the start point (share 0) to the end point (share 1).
    LinearGradient,

    // Changes with the distance from the start point, the center (share 0), to the circle through the end point (share 1).
    RadialGradient,

    // Changes with the direction from the start point, the center, all the way around: share 0 points to the right, 0.25 down,
    // 0.5 to the left and 0.75 up.
    ConicGradient,

    // Changes with the distance from the rectangle from the start point (its top left corner) to the end point (its bottom right corner):
    // the shares are distances in units, 0 on the rectangle and inside it. A soft shadow.
    RectangleGradient,
};

// One color of a gradient and where it is, as a share from 0 to 1. Between two stops the colors mix; before the first stop and after
// the last one the color stays.
struct ColorStop
{
    float share = 0;
    Color color = {0, 0, 0, 0};
};

// What a shape is filled with.
struct Paint
{
    PaintKind kind = PaintKind::Solid;
    Point start = {0, 0};
    Point end = {0, 0};

    // In order of their shares. A solid paint has one stop.
    std::vector<ColorStop> stops;

    // The color of the paint at a point of the picture.
    Color ColorAt(Point point) const;
};

Paint SolidPaint(Color color);
Paint LinearGradient(Point start, Color startColor, Point end, Color endColor);

// The center color fills the circle of the inner radius, and changes to the edge color at the radius.
Paint RadialGradient(Point center, Color centerColor, float radius, Color edgeColor, float innerRadius = 0);

// The colors at the distances from the center that the stops give in units (not shares).
Paint RadialGradientByDistance(Point center, const std::vector<ColorStop>& stopsByDistance);

// The colors at the distances from the rectangle that the stops give in units: a soft shadow around it.
Paint RectangleGradientByDistance(Point topLeft, Point bottomRight, const std::vector<ColorStop>& stopsByDistance);

// The colors in the directions from the center that the stops give in degrees: 0 to the right, 90 down, 180 to the left, 270 up.
Paint ConicGradientByDegrees(Point center, const std::vector<ColorStop>& stopsByDegrees);

}
