#include "SharpArt/CardPerspective.h"

namespace SharpArt
{

// The mapping of a square onto a quadrilateral (Heckbert, "Fundamentals of Texture Mapping and Image Warping", 1989).
CardPerspective::CardPerspective(const std::array<Point, 4>& corners)
{
    const Point& topLeft = corners[0];
    const Point& topRight = corners[1];
    const Point& bottomRight = corners[2];
    const Point& bottomLeft = corners[3];

    // How far the card is from a parallelogram; 0 when it is one, and then the far end is not smaller.
    float slantX = topLeft.x - topRight.x + bottomRight.x - bottomLeft.x;
    float slantY = topLeft.y - topRight.y + bottomRight.y - bottomLeft.y;

    float rightSideX = topRight.x - bottomRight.x;
    float rightSideY = topRight.y - bottomRight.y;
    float bottomSideX = bottomLeft.x - bottomRight.x;
    float bottomSideY = bottomLeft.y - bottomRight.y;
    float determinant = rightSideX * bottomSideY - bottomSideX * rightSideY;

    g = (slantX * bottomSideY - bottomSideX * slantY) / determinant;
    h = (rightSideX * slantY - slantX * rightSideY) / determinant;
    a = topRight.x - topLeft.x + g * topRight.x;
    b = bottomLeft.x - topLeft.x + h * bottomLeft.x;
    c = topLeft.x;
    d = topRight.y - topLeft.y + g * topRight.y;
    e = bottomLeft.y - topLeft.y + h * bottomLeft.y;
    f = topLeft.y;
}

Point CardPerspective::Place(Point pointOnTheCard) const
{
    float u = pointOnTheCard.x;
    float v = pointOnTheCard.y;
    float depth = g * u + h * v + 1;
    return {(a * u + b * v + c) / depth, (d * u + e * v + f) / depth};
}

}
