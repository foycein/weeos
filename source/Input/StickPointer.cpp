#include "Input/StickPointer.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Input
{

namespace
{

// Assumed (docs/audit/OpenPoints/Controllers.md, CTL-07): a stick pushed all the way moves the hand by this many picture widths
// in a second, for the speeds 1 to 5; a stick less than 0.2 from its middle moves nothing, and the move grows with the square of
// the push beyond that, so that small moves are precise.
constexpr auto PictureWidthsPerSecond = std::to_array<float>({0.4f, 0.6f, 0.8f, 1.1f, 1.5f});
constexpr float StickDeadZone = 0.2f;

// Assumed (CTL-08): turning a controller by 40 degrees takes the hand across the picture at the usual speed; the other speeds scale it
// like the stick.
constexpr float PictureWidthInDegrees = 40.0f;

// The picture is 2 wide and 2 high in the pointer space.
constexpr float PictureWidth = 2.0f;
constexpr float PictureEdge = 1.0f;

float SpeedFactor(int speed)
{
    int clampedSpeed = std::clamp(speed, SlowestPointerSpeed, FastestPointerSpeed);
    return PictureWidthsPerSecond[clampedSpeed - SlowestPointerSpeed];
}

}

void StickPointer::Move(Formats::Vector2 stick, double elapsedSeconds)
{
    float push = std::sqrt(stick.x * stick.x + stick.y * stick.y);
    if (push <= StickDeadZone)
        return;

    float pushBeyondDeadZone = std::min(1.0f, (push - StickDeadZone) / (1.0f - StickDeadZone));
    float distance = pushBeyondDeadZone * pushBeyondDeadZone * SpeedFactor(speed) * PictureWidth * static_cast<float>(elapsedSeconds);

    pointer.x += stick.x / push * distance;
    pointer.y += stick.y / push * distance;
    KeepInsidePicture();
}

void StickPointer::Turn(float rightwardDegrees, float upwardDegrees)
{
    float speedScale = SpeedFactor(speed) / SpeedFactor(UsualPointerSpeed);
    float unitsPerDegree = PictureWidth / PictureWidthInDegrees * speedScale;

    pointer.x += rightwardDegrees * unitsPerDegree;
    pointer.y += upwardDegrees * unitsPerDegree;
    KeepInsidePicture();
}

void StickPointer::SetSpeed(int newSpeed)
{
    speed = std::clamp(newSpeed, SlowestPointerSpeed, FastestPointerSpeed);
}

Formats::Vector2 StickPointer::Pointer() const
{
    return pointer;
}

void StickPointer::KeepInsidePicture()
{
    pointer.x = std::clamp(pointer.x, -PictureEdge, PictureEdge);
    pointer.y = std::clamp(pointer.y, -PictureEdge, PictureEdge);
}

}
