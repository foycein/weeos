#include "SharpArt/Paint.h"

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

// The distance of the point from the center, as a share of the distance from the center to the end point.
float ShareOfRadius(Point center, Point end, Point point)
{
    float radius = std::hypot(end.x - center.x, end.y - center.y);
    if (radius == 0)
        return 0;

    return std::hypot(point.x - center.x, point.y - center.y) / radius;
}

constexpr float FullTurnDegrees = 360;
constexpr float DegreesPerRadian = 57.2957795f;

// The direction from the center to the point as a share of a full turn, from 0 to 1: 0 to the right, 0.25 down.
float ShareOfTurn(Point center, Point point)
{
    float degrees = std::atan2(point.y - center.y, point.x - center.x) * DegreesPerRadian;
    if (degrees < 0)
        degrees += FullTurnDegrees;
    return degrees / FullTurnDegrees;
}

Color ColorOfStopsAt(const std::vector<ColorStop>& stops, float share)
{
    if (share <= stops.front().share)
        return stops.front().color;

    for (std::size_t index = 1; index < stops.size(); ++index)
    {
        const ColorStop& before = stops[index - 1];
        const ColorStop& after = stops[index];
        if (share > after.share)
            continue;

        float shareBetween = (share - before.share) / (after.share - before.share);
        return MixColors(before.color, after.color, shareBetween);
    }
    return stops.back().color;
}

}

Color Paint::ColorAt(Point point) const
{
    if (kind == PaintKind::Solid)
        return stops.front().color;

    float share = 0;
    if (kind == PaintKind::LinearGradient)
        share = ShareAlongLine(start, end, point);
    else if (kind == PaintKind::RadialGradient)
        share = ShareOfRadius(start, end, point);
    else
        share = ShareOfTurn(start, point);

    return ColorOfStopsAt(stops, share);
}

Paint SolidPaint(Color color)
{
    Paint paint;
    paint.stops = {{0, color}};
    return paint;
}

Paint LinearGradient(Point start, Color startColor, Point end, Color endColor)
{
    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = start;
    paint.end = end;
    paint.stops = {{0, startColor}, {1, endColor}};
    return paint;
}

Paint RadialGradient(Point center, Color centerColor, float radius, Color edgeColor, float innerRadius)
{
    return RadialGradientByDistance(center, {{innerRadius, centerColor}, {radius, edgeColor}});
}

Paint RadialGradientByDistance(Point center, const std::vector<ColorStop>& stopsByDistance)
{
    float radius = stopsByDistance.back().share;

    Paint paint;
    paint.kind = PaintKind::RadialGradient;
    paint.start = center;
    paint.end = {center.x + radius, center.y};
    for (const ColorStop& stop : stopsByDistance)
        paint.stops.push_back({stop.share / radius, stop.color});
    return paint;
}

Paint ConicGradientByDegrees(Point center, const std::vector<ColorStop>& stopsByDegrees)
{
    Paint paint;
    paint.kind = PaintKind::ConicGradient;
    paint.start = center;
    for (const ColorStop& stop : stopsByDegrees)
        paint.stops.push_back({stop.share / FullTurnDegrees, stop.color});
    return paint;
}

}
