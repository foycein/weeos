#pragma once

#include "Input/RemoteSource.h"
#include "Input/WiiRemoteReports.h"

#include "Formats/Vectors.h"

#include <optional>
#include <span>

namespace Input
{

// Turns the dots that the camera of a Wii Remote sees into the pointer, the roll and the distance, the work of the pointer library
// of the console (docs/audit/OpenPoints/Input.md, INP-02 and INP-06):
//
// * the dots go into camera units: x from -1 to 1 and y from -0.75 to 0.75, y downward;
// * the line through the two dots of the sensor bar gives the roll; the middle of the dots is turned back by the roll;
// * the library's position is 2.27 times (bar - middle), with the bar at (0, -0.2) above the screen and (0, 0.2) below it;
// * a new position moves the pointer only partly while it is within 0.05 of the last one, which keeps a still hand still;
// * the position becomes the pointer of PointerSpace.h for the 16:9 picture.
class InfraredPointer
{
public:
    void SetSensorBarPosition(SensorBarPosition position);

    // The dots of one report, in the order the camera gives them. Two dots are the sensor bar. With one dot, the other one is taken to
    // be where it was the last time both were seen. Without dots, or with one dot and no earlier pair, the bar is not seen.
    void Update(std::span<const InfraredDot> dots);

    bool SeesSensorBar() const;

    // Only while the bar is seen.
    Formats::Vector2 Pointer() const;
    float RollDegrees() const;
    float DistanceInMeters() const;

private:
    std::optional<Formats::Vector2> FindMiddle(std::span<const InfraredDot> dots);
    void MovePosition(Formats::Vector2 newPosition);

    SensorBarPosition sensorBarPosition = SensorBarPosition::BelowScreen;
    bool seesSensorBar = false;

    // From the left dot to the right dot, in camera units, the last time both were seen.
    std::optional<Formats::Vector2> lastDotSpan;
    Formats::Vector2 lastMiddle = {0, 0};

    // The library's position: x to the right, y downward, about -1 to 1.
    std::optional<Formats::Vector2> position;
    float rollRadians = 0;
};

}
