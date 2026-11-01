#pragma once

#include "Graphics/Matrix4.h"

#include "Formats/Vectors.h"

namespace Effects
{

using Formats::Vector2;
using Formats::Vector3;

Vector3 Add(Vector3 first, Vector3 second);
Vector3 Subtract(Vector3 first, Vector3 second);
Vector3 Multiply(Vector3 vector, float factor);
float Dot(Vector3 first, Vector3 second);
Vector3 Cross(Vector3 first, Vector3 second);
float Length(Vector3 vector);
float LengthSquared(Vector3 vector);

// Smaller than this, a vector has no direction.
constexpr float SmallestLengthSquared = 1.19e-7f;

// The vector made one long, or the fallback when it is too short to have a direction.
Vector3 NormalizeOr(Vector3 vector, Vector3 fallback);

// The library's turn by three angles in radians: first about X, then Y, then Z.
Graphics::Matrix4 MakeRotationXyz(Vector3 angles);

// (0, 1, 0) turned by MakeRotationXyz(angles).
Vector3 UpTurnedBy(Vector3 angles);

// The upper three by three part only, without the move.
Graphics::Matrix4 WithoutTranslation(const Graphics::Matrix4& matrix);

// The inverse of a matrix whose fourth row is (0, 0, 0, 1).
Graphics::Matrix4 InvertAffine(const Graphics::Matrix4& matrix);

Vector3 ColumnOf(const Graphics::Matrix4& matrix, int column);

}
