#pragma once

#include "Formats/Vectors.h"

#include <array>
#include <optional>

namespace Layout
{

constexpr float RadiansPerDegree = 0.017453292f;

// Moves, turns and scales points of space. The matrix has three rows of four numbers:
// the first three columns turn and scale, the fourth column moves.
struct TransformMatrix
{
    std::array<std::array<float, 4>, 3> rows = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
};

TransformMatrix MakeTranslation(float x, float y, float z);
TransformMatrix MakeScale(float x, float y, float z);

// The angles are in degrees. A positive angle turns counterclockwise as seen with x to the right and y upward.
TransformMatrix MakeRotationAroundX(float degrees);
TransformMatrix MakeRotationAroundY(float degrees);
TransformMatrix MakeRotationAroundZ(float degrees);

// The result does the second matrix first and then the first one.
TransformMatrix operator*(const TransformMatrix& first, const TransformMatrix& second);

// Moves a point of the flat layout. The point lies at depth zero, and the result keeps only x and y,
// the way an orthographic view does.
Formats::Vector2 TransformPoint(const TransformMatrix& matrix, Formats::Vector2 point);

// Turns and scales a direction. A direction is not moved: the movement of the matrix does not count.
Formats::Vector2 TransformDirection(const TransformMatrix& matrix, Formats::Vector2 direction);

// The opposite of TransformPoint: finds the point of the flat layout that the matrix moves to the given point.
// There is none when the matrix squeezes the plane into a line, for example a pane with the scale 0.
std::optional<Formats::Vector2> FindPointMovedTo(const TransformMatrix& matrix, Formats::Vector2 target);

}
