#pragma once

#include "Formats/ByteReader.h"

namespace Formats
{

struct Vector2
{
    float x;
    float y;
};

struct Vector3
{
    float x;
    float y;
    float z;
};

Vector2 ReadVector2(ByteReader& reader);
Vector3 ReadVector3(ByteReader& reader);

}
