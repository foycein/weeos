#pragma once

#include "Formats/Vectors.h"

#include <array>

namespace Graphics
{

// Moves, turns, scales and projects points of space. A point (x, y, z) is the column (x, y, z, 1); the result of a row is
// the sum of its four numbers times x, y, z and 1. The fourth column moves the point.
struct Matrix4
{
    std::array<std::array<float, 4>, 4> rows = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
};

// The result does the second matrix first and then the first one.
Matrix4 operator*(const Matrix4& first, const Matrix4& second);

Matrix4 MakeTranslation(Formats::Vector3 offset);
Matrix4 MakeScale(Formats::Vector3 factors);

// The angles are in degrees. A positive angle turns counterclockwise when the axis points at the viewer.
Matrix4 MakeRotationAroundX(float degrees);
Matrix4 MakeRotationAroundY(float degrees);
Matrix4 MakeRotationAroundZ(float degrees);

// The view of a camera at the eye that looks at the target: afterwards the eye is at the origin, looking along -z, with the up
// direction upward.
Matrix4 MakeLookAt(Formats::Vector3 eye, Formats::Vector3 target, Formats::Vector3 up);

// A perspective projection into the picture: x and y from -1 to 1 at the edges, the depth from 0 at the near distance to 1 at the far
// one (after dividing by the fourth number). The view angle is the vertical one, in degrees; the aspect is width / height.
Matrix4 MakePerspective(float verticalAngleDegrees, float aspect, float nearDistance, float farDistance);

// Turns a point (with the move) or a direction (without the move). The fourth row is ignored.
Formats::Vector3 TransformPoint(const Matrix4& matrix, Formats::Vector3 point);
Formats::Vector3 TransformDirection(const Matrix4& matrix, Formats::Vector3 direction);

// The matrix for normals that belongs to this one: the inverse of its turning part, transposed. Normals stay at right angles to
// their surface under a scale that differs per axis. The move and the fourth row are left out.
Matrix4 MakeNormalMatrix(const Matrix4& matrix);

}
