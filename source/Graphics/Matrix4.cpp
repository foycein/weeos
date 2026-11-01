#include "Graphics/Matrix4.h"

#include <cmath>

namespace Graphics
{

namespace
{

constexpr float RadiansPerDegree = 0.017453292f;
constexpr int Size = 4;
constexpr int TurningSize = 3;

Formats::Vector3 Subtract(Formats::Vector3 first, Formats::Vector3 second)
{
    return {first.x - second.x, first.y - second.y, first.z - second.z};
}

Formats::Vector3 Cross(Formats::Vector3 first, Formats::Vector3 second)
{
    return {first.y * second.z - first.z * second.y, first.z * second.x - first.x * second.z, first.x * second.y - first.y * second.x};
}

float Dot(Formats::Vector3 first, Formats::Vector3 second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

Formats::Vector3 Normalize(Formats::Vector3 vector)
{
    float length = std::sqrt(Dot(vector, vector));
    return {vector.x / length, vector.y / length, vector.z / length};
}

// The number at row, column of the turning part of the matrix, with the rows and columns counted around in a circle.
float TurningElement(const Matrix4& matrix, int row, int column)
{
    return matrix.rows[row % TurningSize][column % TurningSize];
}

}

Matrix4 operator*(const Matrix4& first, const Matrix4& second)
{
    Matrix4 result;
    for (int row = 0; row < Size; ++row)
    {
        for (int column = 0; column < Size; ++column)
        {
            float sum = 0;
            for (int step = 0; step < Size; ++step)
                sum += first.rows[row][step] * second.rows[step][column];
            result.rows[row][column] = sum;
        }
    }
    return result;
}

Matrix4 MakeTranslation(Formats::Vector3 offset)
{
    Matrix4 matrix;
    matrix.rows[0][3] = offset.x;
    matrix.rows[1][3] = offset.y;
    matrix.rows[2][3] = offset.z;
    return matrix;
}

Matrix4 MakeScale(Formats::Vector3 factors)
{
    Matrix4 matrix;
    matrix.rows[0][0] = factors.x;
    matrix.rows[1][1] = factors.y;
    matrix.rows[2][2] = factors.z;
    return matrix;
}

Matrix4 MakeRotationAroundX(float degrees)
{
    float sine = std::sin(degrees * RadiansPerDegree);
    float cosine = std::cos(degrees * RadiansPerDegree);

    Matrix4 matrix;
    matrix.rows[1][1] = cosine;
    matrix.rows[1][2] = -sine;
    matrix.rows[2][1] = sine;
    matrix.rows[2][2] = cosine;
    return matrix;
}

Matrix4 MakeRotationAroundY(float degrees)
{
    float sine = std::sin(degrees * RadiansPerDegree);
    float cosine = std::cos(degrees * RadiansPerDegree);

    Matrix4 matrix;
    matrix.rows[0][0] = cosine;
    matrix.rows[0][2] = sine;
    matrix.rows[2][0] = -sine;
    matrix.rows[2][2] = cosine;
    return matrix;
}

Matrix4 MakeRotationAroundZ(float degrees)
{
    float sine = std::sin(degrees * RadiansPerDegree);
    float cosine = std::cos(degrees * RadiansPerDegree);

    Matrix4 matrix;
    matrix.rows[0][0] = cosine;
    matrix.rows[0][1] = -sine;
    matrix.rows[1][0] = sine;
    matrix.rows[1][1] = cosine;
    return matrix;
}

Matrix4 MakeLookAt(Formats::Vector3 eye, Formats::Vector3 target, Formats::Vector3 up)
{
    Formats::Vector3 backward = Normalize(Subtract(eye, target));
    Formats::Vector3 right = Normalize(Cross(up, backward));
    Formats::Vector3 upward = Cross(backward, right);

    Matrix4 matrix;
    matrix.rows[0] = {right.x, right.y, right.z, -Dot(right, eye)};
    matrix.rows[1] = {upward.x, upward.y, upward.z, -Dot(upward, eye)};
    matrix.rows[2] = {backward.x, backward.y, backward.z, -Dot(backward, eye)};
    return matrix;
}

Matrix4 MakePerspective(float verticalAngleDegrees, float aspect, float nearDistance, float farDistance)
{
    float focalLength = 1.0f / std::tan(verticalAngleDegrees * RadiansPerDegree / 2);
    float depthRange = nearDistance - farDistance;

    Matrix4 matrix;
    matrix.rows[0] = {focalLength / aspect, 0, 0, 0};
    matrix.rows[1] = {0, focalLength, 0, 0};
    matrix.rows[2] = {0, 0, farDistance / depthRange, nearDistance * farDistance / depthRange};
    matrix.rows[3] = {0, 0, -1, 0};
    return matrix;
}

Formats::Vector3 TransformPoint(const Matrix4& matrix, Formats::Vector3 point)
{
    Formats::Vector3 turned = TransformDirection(matrix, point);
    return {turned.x + matrix.rows[0][3], turned.y + matrix.rows[1][3], turned.z + matrix.rows[2][3]};
}

Formats::Vector3 TransformDirection(const Matrix4& matrix, Formats::Vector3 direction)
{
    const auto& rows = matrix.rows;
    return {
        rows[0][0] * direction.x + rows[0][1] * direction.y + rows[0][2] * direction.z,
        rows[1][0] * direction.x + rows[1][1] * direction.y + rows[1][2] * direction.z,
        rows[2][0] * direction.x + rows[2][1] * direction.y + rows[2][2] * direction.z,
    };
}

// The inverse transposed is the matrix of cofactors divided by the determinant.
Matrix4 MakeNormalMatrix(const Matrix4& matrix)
{
    Matrix4 cofactors;
    for (int row = 0; row < TurningSize; ++row)
    {
        for (int column = 0; column < TurningSize; ++column)
        {
            float product = TurningElement(matrix, row + 1, column + 1) * TurningElement(matrix, row + 2, column + 2);
            float crossProduct = TurningElement(matrix, row + 1, column + 2) * TurningElement(matrix, row + 2, column + 1);
            cofactors.rows[row][column] = product - crossProduct;
        }
    }

    float determinant = 0;
    for (int column = 0; column < TurningSize; ++column)
        determinant += matrix.rows[0][column] * cofactors.rows[0][column];

    Matrix4 normalMatrix;
    for (int row = 0; row < TurningSize; ++row)
    {
        for (int column = 0; column < TurningSize; ++column)
            normalMatrix.rows[row][column] = cofactors.rows[row][column] / determinant;
    }
    return normalMatrix;
}

}
