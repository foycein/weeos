#include "Formats/Vectors.h"

namespace Formats
{

Vector2 ReadVector2(ByteReader& reader)
{
    Vector2 vector;
    vector.x = reader.ReadFloat();
    vector.y = reader.ReadFloat();
    return vector;
}

Vector3 ReadVector3(ByteReader& reader)
{
    Vector3 vector;
    vector.x = reader.ReadFloat();
    vector.y = reader.ReadFloat();
    vector.z = reader.ReadFloat();
    return vector;
}

}
