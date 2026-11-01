#pragma once

#include "Formats/RgbaImage.h"

namespace Media
{

// Makes a picture smaller by a whole factor: every pixel of the result is the average of factor x factor pixels (fewer at the
// right and bottom edges when the size is not a multiple of the factor).
Formats::RgbaImage ShrinkPicture(const Formats::RgbaImage& picture, int factor);

// Turns a picture by a quarter turn clockwise (right), counterclockwise (left), or by half a turn.
Formats::RgbaImage TurnPictureRight(const Formats::RgbaImage& picture);
Formats::RgbaImage TurnPictureLeft(const Formats::RgbaImage& picture);
Formats::RgbaImage TurnPictureUpsideDown(const Formats::RgbaImage& picture);

}
