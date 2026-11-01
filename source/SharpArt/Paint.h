#pragma once

#include "SharpArt/VectorPath.h"

#include "Formats/RgbaImage.h"

namespace SharpArt
{

using Color = Formats::Rgba8Color;

enum class PaintKind
{
    // One color everywhere.
    Solid,

    // From the first color at the start point to the second color at the end point, along the line between them.
    LinearGradient,

    // The first color inside the circle of the inner radius around the start point (the center), changing to the second color on
    // the circle through the end point.
    RadialGradient,
};

// What a shape is filled with.
struct Paint
{
    PaintKind kind = PaintKind::Solid;
    Color firstColor = {0, 0, 0, 0};
    Color secondColor = {0, 0, 0, 0};
    Point start = {0, 0};
    Point end = {0, 0};
    float innerRadius = 0;

    // The color of the paint at a point of the picture.
    Color ColorAt(Point point) const;
};

Paint SolidPaint(Color color);
Paint LinearGradient(Point start, Color startColor, Point end, Color endColor);
Paint RadialGradient(Point center, Color centerColor, float radius, Color edgeColor, float innerRadius = 0);

}
