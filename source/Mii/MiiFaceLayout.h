#pragma once

#include "Mii/MiiRecord.h"

#include "Formats/Vectors.h"

#include <array>
#include <vector>

namespace Mii
{

// The pictures that the face texture is made of.
enum class FaceFeature
{
    Mustache,
    Mouth,
    Eyebrow,
    Eye,
    Mole,
};

// One picture of the face texture as a quad. The corners are in pixels of the face texture, x to the right and y downward,
// in the order left top, right top, right bottom, left bottom of the picture before it is turned.
struct FaceFeatureQuad
{
    FaceFeature feature;
    std::array<Formats::Vector2, 4> corners;
    std::array<Formats::Vector2, 4> textureCoordinates;
};

// The eight quads of a face texture of size x size pixels, in the order they are drawn: the mustache's left and right half,
// the mouth, the left and right eyebrow, the left and right eye, the mole (docs/research/Mii.md section 7.2).
// "Left" is the left of the picture.
std::vector<FaceFeatureQuad> LayOutFace(const MiiRecord& record, float textureSize);

}
