#pragma once

#include "SharpArt/VectorPath.h"

#include <array>

namespace SharpArt
{

// A flat card seen at an angle: the square from (0, 0) to (1, 1) on the card, laid onto the four corners where a picture shows the card.
// Straight lines stay straight, and the far end of the card is smaller than the near one.
class CardPerspective
{
public:
    // The corners as the picture shows them, in order: top left, top right, bottom right, bottom left.
    explicit CardPerspective(const std::array<Point, 4>& corners);

    // Where the point of the card, from (0, 0) to (1, 1), lies in the picture.
    Point Place(Point pointOnTheCard) const;

private:
    // x = (a u + b v + c) / (g u + h v + 1) and y = (d u + e v + f) / (g u + h v + 1), for the point (u, v) of the card.
    float a = 1;
    float b = 0;
    float c = 0;
    float d = 0;
    float e = 1;
    float f = 0;
    float g = 0;
    float h = 0;
};

}
