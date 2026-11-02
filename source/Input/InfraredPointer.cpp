#include "Input/InfraredPointer.h"

#include <cmath>
#include <numbers>

namespace Input
{

namespace
{

// The pointer library's camera units: a pixel is 2 / 1024, and the middle of the picture is at (0.999, 0.749).
constexpr float CameraUnitsPerPixel = 2.0f / 1024.0f;
constexpr float CameraMiddleX = 0.999f;
constexpr float CameraMiddleY = 0.749f;

// The bar is 0.2 camera units above or below where the remote points when it points at the middle of the screen; the scale from
// camera units to the library's position is 1.25 / 0.55 for both.
constexpr float SensorBarHeight = 0.2f;
constexpr float CameraToPosition = 1.25f / 0.55f;

// The menu sets the play radius 0.05 and the sensitivity 1.0 for every channel.
constexpr float PlayRadius = 0.05f;
constexpr float PlaySensitivity = 1.0f;
constexpr int PlayExponent = 4;

// The menu's pointer: the position times 1.2 times half the height of the canvas, x stretched by 1.15 on a wide screen, in a canvas
// of 832 x 456. Divided by half the canvas that is our pointer space.
constexpr float PointerScale = 1.2f;
constexpr float CanvasHalfHeight = 228.0f;
constexpr float WideCanvasHalfWidth = 416.0f;
constexpr float WideScreenStretch = 1.15f;

// The distance is 0.2 / tan(21 degrees) / the span of the dots: the bar's two groups of lights are 0.2 m apart and
// half of the camera picture is 21 degrees wide.
constexpr float SensorBarWidthInMeters = 0.2f;
constexpr float TangentOfHalfCameraAngle = 0.38386f;

Formats::Vector2 ToCameraUnits(InfraredDot dot)
{
    return {dot.x * CameraUnitsPerPixel - CameraMiddleX, dot.y * CameraUnitsPerPixel - CameraMiddleY};
}

float Length(Formats::Vector2 vector)
{
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

Formats::Vector2 Subtract(Formats::Vector2 first, Formats::Vector2 second)
{
    return {first.x - second.x, first.y - second.y};
}

Formats::Vector2 Add(Formats::Vector2 first, Formats::Vector2 second)
{
    return {first.x + second.x, first.y + second.y};
}

Formats::Vector2 Scale(Formats::Vector2 vector, float factor)
{
    return {vector.x * factor, vector.y * factor};
}

// A turned remote sees the picture turned the other way. Turning the middle back by the roll gives where it would be on a level remote.
Formats::Vector2 TurnBack(Formats::Vector2 point, float rollRadians)
{
    float cosine = std::cos(rollRadians);
    float sine = std::sin(rollRadians);
    return {point.x * cosine + point.y * sine, -point.x * sine + point.y * cosine};
}

}

void InfraredPointer::SetSensorBarPosition(SensorBarPosition newPosition)
{
    sensorBarPosition = newPosition;
}

void InfraredPointer::Update(std::span<const InfraredDot> dots)
{
    std::optional<Formats::Vector2> middle = FindMiddle(dots);
    if (!middle)
    {
        seesSensorBar = false;
        position.reset();
        return;
    }

    seesSensorBar = true;
    lastMiddle = *middle;

    float barY = sensorBarPosition == SensorBarPosition::AboveScreen ? -SensorBarHeight : SensorBarHeight;
    Formats::Vector2 bar = {0, barY};
    Formats::Vector2 levelMiddle = TurnBack(*middle, rollRadians);
    MovePosition(Scale(Subtract(bar, levelMiddle), CameraToPosition));
}

// shortcut: with more than two dots the first two are the bar; enough for a sensor bar or a Dolphin Bar without reflections nearby,
// pick the pair that fits the last span best if reflections ever get in the way.
std::optional<Formats::Vector2> InfraredPointer::FindMiddle(std::span<const InfraredDot> dots)
{
    if (dots.size() >= 2)
    {
        Formats::Vector2 left = ToCameraUnits(dots[0]);
        Formats::Vector2 right = ToCameraUnits(dots[1]);
        if (left.x > right.x)
            std::swap(left, right);

        Formats::Vector2 dotSpan = Subtract(right, left);
        lastDotSpan = dotSpan;
        rollRadians = std::atan2(dotSpan.y, dotSpan.x);
        return Scale(Add(left, right), 0.5f);
    }

    if (dots.size() == 1 && lastDotSpan)
    {
        Formats::Vector2 dot = ToCameraUnits(dots[0]);
        Formats::Vector2 halfSpan = Scale(*lastDotSpan, 0.5f);
        Formats::Vector2 middleIfLeftDot = Add(dot, halfSpan);
        Formats::Vector2 middleIfRightDot = Subtract(dot, halfSpan);

        float distanceIfLeftDot = Length(Subtract(middleIfLeftDot, lastMiddle));
        float distanceIfRightDot = Length(Subtract(middleIfRightDot, lastMiddle));
        return distanceIfLeftDot <= distanceIfRightDot ? middleIfLeftDot : middleIfRightDot;
    }

    return std::nullopt;
}

void InfraredPointer::MovePosition(Formats::Vector2 newPosition)
{
    if (!position)
    {
        position = newPosition;
        return;
    }

    Formats::Vector2 change = Subtract(newPosition, *position);
    float distance = Length(change);

    float weight = 1;
    if (distance < PlayRadius)
        weight = std::pow(distance / PlayRadius, static_cast<float>(PlayExponent)) * PlaySensitivity;

    position = Add(*position, Scale(change, weight));
}

bool InfraredPointer::SeesSensorBar() const
{
    return seesSensorBar;
}

Formats::Vector2 InfraredPointer::Pointer() const
{
    if (!position)
        return {0, 0};

    float x = position->x * PointerScale * CanvasHalfHeight * WideScreenStretch / WideCanvasHalfWidth;
    float y = -position->y * PointerScale;
    return {x, y};
}

float InfraredPointer::RollDegrees() const
{
    return rollRadians * 180.0f / std::numbers::pi_v<float>;
}

float InfraredPointer::DistanceInMeters() const
{
    if (!lastDotSpan)
        return UsualDistanceInMeters;

    float span = Length(*lastDotSpan);
    if (span <= 0)
        return UsualDistanceInMeters;

    return SensorBarWidthInMeters / TangentOfHalfCameraAngle / span;
}

}
