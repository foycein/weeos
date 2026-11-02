#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/TplTexture.h"
#include "Formats/Vectors.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Mii
{

// The 3D parts of a head in RFL_Res.dat.
enum class ShapePart
{
    Beard,
    Faceline,
    Forehead,
    Glasses,
    Hair,
    Mask,
    Nose,
    Noseline,
    Cap,
};

// The pictures in RFL_Res.dat.
enum class TexturePart
{
    Eye,
    Eyebrow,
    FacelineMakeUp,
    Glasses,
    Mole,
    Mouth,
    Mustache,
    Noseline,
    Cap,
};

// A position as RFL_Res.dat stores it, in 1/256 of a unit.
struct StoredPosition
{
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
};

// One corner of a triangle: the numbers of its position, normal and texture coordinate in the arrays of the shape.
struct ShapeCorner
{
    std::uint8_t position;
    std::uint8_t normal;
    std::uint8_t textureCoordinate;
};

// One 3D part. An empty shape (a bald head has no hair) has no positions and no triangles.
struct MiiShape
{
    std::vector<StoredPosition> positions;
    std::vector<Formats::Vector3> normals;
    std::vector<Formats::Vector2> textureCoordinates;

    // Three corners per triangle. Seen from outside, the corners of a triangle go around clockwise.
    std::vector<ShapeCorner> triangleCorners;

    // Only a faceline has these: where the nose and glasses, the beard and the hair are placed on that face.
    std::vector<Formats::Vector3> anchorPoints;
};

struct MiiTexture
{
    Formats::RgbaImage image;
    Formats::TextureWrap horizontalWrap;
    Formats::TextureWrap verticalWrap;
};

// The faceline's anchor points.
constexpr std::size_t NoseAnchor = 0;
constexpr std::size_t BeardAnchor = 1;
constexpr std::size_t HairAnchor = 2;

// RFL_Res.dat, the parts of the faces. It is content 0xb of the System Menu.
class MiiResources
{
public:
    // Reads the table of the sections. Throws a FormatError when it does not fit into the file.
    explicit MiiResources(Formats::Bytes fileBytes);

    // Throws a FormatError when the entry does not exist or its data is broken.
    MiiShape ReadShape(ShapePart part, int entry) const;
    MiiTexture ReadTexture(TexturePart part, int entry) const;

    int EntryCount(ShapePart part) const;
    int EntryCount(TexturePart part) const;

private:
    struct Section
    {
        std::size_t entryCount;
        std::vector<std::size_t> entryOffsets;
    };

    Formats::ByteView EntryBytes(int sectionNumber, int entry) const;

    Formats::Bytes bytes;
    std::vector<Section> sections;
};

}
