#include "SharpArt/Paint.h"

#include <algorithm>
#include <cmath>

namespace SharpArt
{

namespace
{

Formats::Byte MixByte(Formats::Byte first, Formats::Byte second, float share)
{
    float mixed = first + (second - first) * share;
    return static_cast<Formats::Byte>(std::lround(mixed));
}

Color MixColors(Color first, Color second, float share)
{
    Color mixed;
    mixed.red = MixByte(first.red, second.red, share);
    mixed.green = MixByte(first.green, second.green, share);
    mixed.blue = MixByte(first.blue, second.blue, share);
    mixed.alpha = MixByte(first.alpha, second.alpha, share);
    return mixed;
}

// How far the point lies from the start towards the end, measured along the line between them: 0 at the start, 1 at the end.
float ShareAlongLine(Point start, Point end, Point point)
{
    float lineX = end.x - start.x;
    float lineY = end.y - start.y;
    float lengthSquared = lineX * lineX + lineY * lineY;
    if (lengthSquared == 0)
        return 0;

    float projection = (point.x - start.x) * lineX + (point.y - start.y) * lineY;
    return projection / lengthSquared;
}

// 0 inside the inner circle, 1 on the outer circle through onTheCircle, and in between by the distance from the center.
float ShareOfRadius(Point center, Point onTheCircle, float innerRadius, Point point)
{
    float radius = std::hypot(onTheCircle.x - center.x, onTheCircle.y - center.y);
    if (radius <= innerRadius)
        return 0;

    float distance = std::hypot(point.x - center.x, point.y - center.y);
    return (distance - innerRadius) / (radius - innerRadius);
}

}

Color Paint::ColorAt(Point point) const
{
    if (kind == PaintKind::Solid)
        return firstColor;

    float share = 0;
    if (kind == PaintKind::LinearGradient)
        share = ShareAlongLine(start, end, point);
    else
        share = ShareOfRadius(start, end, innerRadius, point);

    return MixColors(firstColor, secondColor, std::clamp(share, 0.0f, 1.0f));
}

Paint SolidPaint(Color color)
{
    Paint paint;
    paint.firstColor = color;
    return paint;
}

Paint LinearGradient(Point start, Color startColor, Point end, Color endColor)
{
    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = start;
    paint.firstColor = startColor;
    paint.end = end;
    paint.secondColor = endColor;
    return paint;
}

Paint RadialGradient(Point center, Color centerColor, float radius, Color edgeColor, float innerRadius)
{
    Paint paint;
    paint.kind = PaintKind::RadialGradient;
    paint.start = center;
    paint.firstColor = centerColor;
    paint.end = {center.x + radius, center.y};
    paint.secondColor = edgeColor;
    paint.innerRadius = innerRadius;
    return paint;
}

}
