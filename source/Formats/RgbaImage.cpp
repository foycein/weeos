#include "Formats/RgbaImage.h"

#include "Formats/ByteReader.h"

namespace Formats
{

Rgba8Color ReadRgba8Color(ByteReader& reader)
{
    Rgba8Color color;
    color.red = reader.ReadByte();
    color.green = reader.ReadByte();
    color.blue = reader.ReadByte();
    color.alpha = reader.ReadByte();
    return color;
}

}
