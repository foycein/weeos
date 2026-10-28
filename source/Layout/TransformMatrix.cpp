#include "Layout/TransformMatrix.h"

#include <cmath>

namespace Layout
{

namespace
{

constexpr std::size_t RowCount = 3;
constexpr std::size_t ColumnCount = 4;
constexpr std::size_t MovementColumn = 3;

}

TransformMatrix MakeTranslation(float x, float y, float z)
{
    TransformMatrix matrix;
    matrix.rows[0][MovementColumn] = x;
    matrix.rows[1][MovementColumn] = y;
    matrix.rows[2][MovementColumn] = z;
    return matrix;
}

TransformMatrix MakeScale(float x, float y, float z)
{
    TransformMatrix matrix;
    matrix.rows[0][0] = x;
    matrix.rows[1][1] = y;
    matrix.rows[2][2] = z;
    return matrix;
}

TransformMatrix MakeRotationAroundX(float degrees)
{
    float radians = RadiansPerDegree * degrees;
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    TransformMatrix matrix;
    matrix.rows[1][1] = cosine;
    matrix.rows[1][2] = -sine;
    matrix.rows[2][1] = sine;
    matrix.rows[2][2] = cosine;
    return matrix;
}

TransformMatrix MakeRotationAroundY(float degrees)
{
    float radians = RadiansPerDegree * degrees;
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    TransformMatrix matrix;
    matrix.rows[0][0] = cosine;
    matrix.rows[0][2] = sine;
    matrix.rows[2][0] = -sine;
    matrix.rows[2][2] = cosine;
    return matrix;
}

TransformMatrix MakeRotationAroundZ(float degrees)
{
    float radians = RadiansPerDegree * degrees;
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    TransformMatrix matrix;
    matrix.rows[0][0] = cosine;
    matrix.rows[0][1] = -sine;
    matrix.rows[1][0] = sine;
    matrix.rows[1][1] = cosine;
    return matrix;
}

TransformMatrix operator*(const TransformMatrix& first, const TransformMatrix& second)
{
    TransformMatrix product;
    for (std::size_t row = 0; row < RowCount; ++row)
    {
        for (std::size_t column = 0; column < ColumnCount; ++column)
        {
            float sum = 0;
            for (std::size_t step = 0; step < RowCount; ++step)
                sum += first.rows[row][step] * second.rows[step][column];

            product.rows[row][column] = sum;
        }

        // Both matrices have a fourth row of 0, 0, 0, 1, so the movement of the first matrix is simply added.
        product.rows[row][MovementColumn] += first.rows[row][MovementColumn];
    }
    return product;
}

Formats::Vector2 TransformPoint(const TransformMatrix& matrix, Formats::Vector2 point)
{
    Formats::Vector2 moved;
    moved.x = matrix.rows[0][0] * point.x + matrix.rows[0][1] * point.y + matrix.rows[0][MovementColumn];
    moved.y = matrix.rows[1][0] * point.x + matrix.rows[1][1] * point.y + matrix.rows[1][MovementColumn];
    return moved;
}

std::optional<Formats::Vector2> FindPointMovedTo(const TransformMatrix& matrix, Formats::Vector2 target)
{
    float xFromX = matrix.rows[0][0];
    float xFromY = matrix.rows[0][1];
    float yFromX = matrix.rows[1][0];
    float yFromY = matrix.rows[1][1];

    float determinant = xFromX * yFromY - xFromY * yFromX;
    if (determinant == 0.0f)
        return std::nullopt;

    float xToCover = target.x - matrix.rows[0][MovementColumn];
    float yToCover = target.y - matrix.rows[1][MovementColumn];

    Formats::Vector2 point;
    point.x = (yFromY * xToCover - xFromY * yToCover) / determinant;
    point.y = (xFromX * yToCover - yFromX * xToCover) / determinant;
    return point;
}

}
