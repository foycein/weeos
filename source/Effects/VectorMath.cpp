#include "Effects/VectorMath.h"

#include <cmath>

namespace Effects
{

Vector3 Add(Vector3 first, Vector3 second)
{
    return {first.x + second.x, first.y + second.y, first.z + second.z};
}

Vector3 Subtract(Vector3 first, Vector3 second)
{
    return {first.x - second.x, first.y - second.y, first.z - second.z};
}

Vector3 Multiply(Vector3 vector, float factor)
{
    return {vector.x * factor, vector.y * factor, vector.z * factor};
}

float Dot(Vector3 first, Vector3 second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

Vector3 Cross(Vector3 first, Vector3 second)
{
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x,
    };
}

float LengthSquared(Vector3 vector)
{
    return Dot(vector, vector);
}

float Length(Vector3 vector)
{
    return std::sqrt(LengthSquared(vector));
}

Vector3 NormalizeOr(Vector3 vector, Vector3 fallback)
{
    float lengthSquared = LengthSquared(vector);
    if (lengthSquared < SmallestLengthSquared)
        return fallback;

    return Multiply(vector, 1 / std::sqrt(lengthSquared));
}

Graphics::Matrix4 MakeRotationXyz(Vector3 angles)
{
    float sinX = std::sin(angles.x);
    float cosX = std::cos(angles.x);
    float sinY = std::sin(angles.y);
    float cosY = std::cos(angles.y);
    float sinZ = std::sin(angles.z);
    float cosZ = std::cos(angles.z);

    Graphics::Matrix4 aroundX;
    aroundX.rows[1] = {0, cosX, -sinX, 0};
    aroundX.rows[2] = {0, sinX, cosX, 0};

    Graphics::Matrix4 aroundY;
    aroundY.rows[0] = {cosY, 0, sinY, 0};
    aroundY.rows[2] = {-sinY, 0, cosY, 0};

    Graphics::Matrix4 aroundZ;
    aroundZ.rows[0] = {cosZ, -sinZ, 0, 0};
    aroundZ.rows[1] = {sinZ, cosZ, 0, 0};

    return aroundZ * aroundY * aroundX;
}

Vector3 UpTurnedBy(Vector3 angles)
{
    return Graphics::TransformDirection(MakeRotationXyz(angles), {0, 1, 0});
}

Graphics::Matrix4 WithoutTranslation(const Graphics::Matrix4& matrix)
{
    Graphics::Matrix4 result = matrix;
    for (int row = 0; row < 3; ++row)
        result.rows[row][3] = 0;
    return result;
}

// The rows of the inverse of a three by three matrix are the cross products of its columns, divided by the determinant.
Graphics::Matrix4 InvertAffine(const Graphics::Matrix4& matrix)
{
    Vector3 firstColumn = ColumnOf(matrix, 0);
    Vector3 secondColumn = ColumnOf(matrix, 1);
    Vector3 thirdColumn = ColumnOf(matrix, 2);

    Vector3 firstRow = Cross(secondColumn, thirdColumn);
    float determinant = Dot(firstColumn, firstRow);
    if (std::abs(determinant) < SmallestLengthSquared)
        return {};

    float factor = 1 / determinant;
    firstRow = Multiply(firstRow, factor);
    Vector3 secondRow = Multiply(Cross(thirdColumn, firstColumn), factor);
    Vector3 thirdRow = Multiply(Cross(firstColumn, secondColumn), factor);

    Vector3 translation = ColumnOf(matrix, 3);
    Graphics::Matrix4 inverse;
    inverse.rows[0] = {firstRow.x, firstRow.y, firstRow.z, -Dot(firstRow, translation)};
    inverse.rows[1] = {secondRow.x, secondRow.y, secondRow.z, -Dot(secondRow, translation)};
    inverse.rows[2] = {thirdRow.x, thirdRow.y, thirdRow.z, -Dot(thirdRow, translation)};
    return inverse;
}

Vector3 ColumnOf(const Graphics::Matrix4& matrix, int column)
{
    return {matrix.rows[0][column], matrix.rows[1][column], matrix.rows[2][column]};
}

}
