#pragma once

#include "Formats/Vectors.h"

namespace Input
{

// How fast a stick moves the hand, 1 (slow) to 5 (fast). The setting of PC Settings, Controls.
constexpr int SlowestPointerSpeed = 1;
constexpr int FastestPointerSpeed = 5;
constexpr int UsualPointerSpeed = 3;

// A hand that a stick moves like a mouse: the further the stick is pushed, the faster the hand goes. The hand stays inside the
// picture (PointerSpace.h) and starts in its middle.
class StickPointer
{
public:
    // The stick from -1 to 1, y upward, and the seconds since the last move.
    void Move(Formats::Vector2 stick, double elapsedSeconds);

    // A turn of a controller with a gyroscope, in degrees: at the usual speed a turn of 40 degrees crosses the picture.
    void Turn(float rightwardDegrees, float upwardDegrees);

    void SetSpeed(int speed);

    Formats::Vector2 Pointer() const;

private:
    void KeepInsidePicture();

    Formats::Vector2 pointer = {0, 0};
    int speed = UsualPointerSpeed;
};

}
