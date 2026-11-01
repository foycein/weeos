#include "Mii/MiiResources.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/TextureDecoder.h"
#include "Formats/TextureFormat.h"

#include <format>
#include <string_view>

namespace Mii
{

namespace
{

// The sections of RFL_Res.dat are in the alphabetical order of their parts.
constexpr int BeardShapeSection = 0;
constexpr int EyeTextureSection = 1;
constexpr int EyebrowTextureSection = 2;
constexpr int FacelineShapeSection = 3;
constexpr int FacelineTextureSection = 4;
constexpr int ForeheadShapeSection = 5;
constexpr int GlassesShapeSection = 6;
constexpr int GlassesTextureSection = 7;
constexpr int HairShapeSection = 8;
constexpr int MaskShapeSection = 9;
constexpr int MoleTextureSection = 10;
constexpr int MouthTextureSection = 11;
constexpr int MustacheTextureSection = 12;
constexpr int NoseShapeSection = 13;
constexpr int NoselineShapeSection = 14;
constexpr int NoselineTextureSection = 15;
constexpr int CapShapeSection = 16;
constexpr int CapTextureSection = 17;
constexpr int SectionCount = 18;

constexpr std::size_t FileHeaderSize = 4;
constexpr std::size_t SectionHeaderSize = 4;
constexpr std::size_t ShapeTagSize = 4;
constexpr std::size_t AnchorPointCount = 3;

constexpr float NormalUnit = 16384.0f;
constexpr float TextureCoordinateUnit = 8192.0f;

constexpr std::uint8_t TrianglesPrimitive = 0x90;
constexpr std::uint8_t TriangleStripPrimitive = 0x98;
constexpr std::size_t CornersPerTriangle = 3;

constexpr std::size_t TextureFormatOffset = 0x00;
constexpr std::size_t TextureWidthOffset = 0x02;
constexpr std::size_t TextureHeightOffset = 0x04;
constexpr std::size_t TextureHorizontalWrapOffset = 0x06;
constexpr std::size_t TextureVerticalWrapOffset = 0x07;
constexpr std::size_t TextureDataOffsetOffset = 0x1C;
constexpr std::uint32_t HighestWrapNumber = 2;

int SectionOf(ShapePart part)
{
    switch (part)
    {
    case ShapePart::Beard:
        return BeardShapeSection;
    case ShapePart::Faceline:
        return FacelineShapeSection;
    case ShapePart::Forehead:
        return ForeheadShapeSection;
    case ShapePart::Glasses:
        return GlassesShapeSection;
    case ShapePart::Hair:
        return HairShapeSection;
    case ShapePart::Mask:
        return MaskShapeSection;
    case ShapePart::Nose:
        return NoseShapeSection;
    case ShapePart::Noseline:
        return NoselineShapeSection;
    case ShapePart::Cap:
        return CapShapeSection;
    }
    throw Formats::FormatError("Mii resources: unknown shape part");
}

int SectionOf(TexturePart part)
{
    switch (part)
    {
    case TexturePart::Eye:
        return EyeTextureSection;
    case TexturePart::Eyebrow:
        return EyebrowTextureSection;
    case TexturePart::FacelineMakeUp:
        return FacelineTextureSection;
    case TexturePart::Glasses:
        return GlassesTextureSection;
    case TexturePart::Mole:
        return MoleTextureSection;
    case TexturePart::Mouth:
        return MouthTextureSection;
    case TexturePart::Mustache:
        return MustacheTextureSection;
    case TexturePart::Noseline:
        return NoselineTextureSection;
    case TexturePart::Cap:
        return CapTextureSection;
    }
    throw Formats::FormatError("Mii resources: unknown texture part");
}

std::string_view TagOf(ShapePart part)
{
    switch (part)
    {
    case ShapePart::Beard:
        return "berd";
    case ShapePart::Faceline:
        return "face";
    case ShapePart::Forehead:
        return "frhd";
    case ShapePart::Glasses:
        return "glas";
    case ShapePart::Hair:
        return "hair";
    case ShapePart::Mask:
        return "mask";
    case ShapePart::Nose:
        return "nose";
    case ShapePart::Noseline:
        return "nsln";
    case ShapePart::Cap:
        return "cap_";
    }
    return "";
}

bool HasTextureCoordinates(ShapePart part)
{
    return part == ShapePart::Faceline || part == ShapePart::Cap || part == ShapePart::Noseline || part == ShapePart::Mask
        || part == ShapePart::Glasses;
}

ShapeCorner ReadCorner(Formats::ByteReader& reader, const MiiShape& shape, bool hasTextureCoordinates)
{
    ShapeCorner corner = {};
    corner.position = reader.ReadByte();
    corner.normal = reader.ReadByte();
    if (hasTextureCoordinates)
        corner.textureCoordinate = reader.ReadByte();

    bool isInRange = corner.position < shape.positions.size() && corner.normal < shape.normals.size();
    if (hasTextureCoordinates && corner.textureCoordinate >= shape.textureCoordinates.size())
        isInRange = false;
    if (!isInRange)
        reader.Fail("a corner names a position, normal or texture coordinate that does not exist");

    return corner;
}

// A strip makes a triangle of every three corners in a row. Every second triangle has its first two corners swapped,
// so that all of them go around the same way.
void AddStripTriangles(const std::vector<ShapeCorner>& strip, std::vector<ShapeCorner>& triangleCorners)
{
    for (std::size_t first = 0; first + 2 < strip.size(); ++first)
    {
        bool isSecondOfPair = first % 2 == 1;
        if (isSecondOfPair)
        {
            triangleCorners.push_back(strip[first + 1]);
            triangleCorners.push_back(strip[first]);
        }
        else
        {
            triangleCorners.push_back(strip[first]);
            triangleCorners.push_back(strip[first + 1]);
        }
        triangleCorners.push_back(strip[first + 2]);
    }
}

void ReadPrimitives(Formats::ByteReader& reader, MiiShape& shape, bool hasTextureCoordinates)
{
    int primitiveCount = reader.ReadByte();
    for (int primitiveNumber = 0; primitiveNumber < primitiveCount; ++primitiveNumber)
    {
        int cornerCount = reader.ReadByte();
        std::uint8_t primitive = reader.ReadByte();

        std::vector<ShapeCorner> corners;
        for (int cornerNumber = 0; cornerNumber < cornerCount; ++cornerNumber)
            corners.push_back(ReadCorner(reader, shape, hasTextureCoordinates));

        if (primitive == TriangleStripPrimitive)
            AddStripTriangles(corners, shape.triangleCorners);
        else if (primitive == TrianglesPrimitive && corners.size() % CornersPerTriangle == 0)
            shape.triangleCorners.insert(shape.triangleCorners.end(), corners.begin(), corners.end());
        else
            reader.Fail(std::format("the primitive {:#x} with {} corners is not a list of triangles", primitive, cornerCount));
    }
}

Formats::Vector3 ReadFixedVector(Formats::ByteReader& reader, float unit)
{
    float x = reader.ReadInt16() / unit;
    float y = reader.ReadInt16() / unit;
    float z = reader.ReadInt16() / unit;
    return {x, y, z};
}

Formats::TextureWrap ToWrap(std::uint32_t number, Formats::ByteReader& reader)
{
    if (number > HighestWrapNumber)
        reader.Fail(std::format("the wrap mode {} is not known", number));

    return static_cast<Formats::TextureWrap>(number);
}

}

MiiResources::MiiResources(Formats::Bytes fileBytes)
    : bytes(std::move(fileBytes))
{
    Formats::ByteReader reader(bytes, "Mii resources");
    reader.Skip(FileHeaderSize);

    std::vector<std::size_t> sectionOffsets;
    for (int sectionNumber = 0; sectionNumber < SectionCount; ++sectionNumber)
        sectionOffsets.push_back(reader.ReadUint32());

    for (std::size_t sectionOffset : sectionOffsets)
    {
        reader.MoveTo(sectionOffset);
        Section section;
        section.entryCount = reader.ReadUint16();
        reader.Skip(sizeof(std::uint16_t));

        std::size_t tableSize = (section.entryCount + 1) * sizeof(std::uint32_t);
        std::size_t entriesStart = sectionOffset + SectionHeaderSize + tableSize;
        for (std::size_t offsetNumber = 0; offsetNumber <= section.entryCount; ++offsetNumber)
            section.entryOffsets.push_back(entriesStart + reader.ReadUint32());

        if (section.entryOffsets.back() > bytes.size())
            reader.Fail("a section reaches past the end of the file");

        sections.push_back(section);
    }
}

MiiShape MiiResources::ReadShape(ShapePart part, int entry) const
{
    Formats::ByteReader reader(EntryBytes(SectionOf(part), entry), std::format("Mii shape {} {}", TagOf(part), entry));

    std::string tag = reader.ReadText(ShapeTagSize);
    if (tag != TagOf(part))
        reader.Fail(std::format("the tag is \"{}\" instead of \"{}\"", tag, TagOf(part)));

    MiiShape shape;
    if (part == ShapePart::Faceline)
    {
        for (std::size_t pointNumber = 0; pointNumber < AnchorPointCount; ++pointNumber)
            shape.anchorPoints.push_back(Formats::ReadVector3(reader));
    }

    int positionCount = reader.ReadUint16();
    if (positionCount == 0)
        return shape;

    for (int positionNumber = 0; positionNumber < positionCount; ++positionNumber)
    {
        StoredPosition position;
        position.x = reader.ReadInt16();
        position.y = reader.ReadInt16();
        position.z = reader.ReadInt16();
        shape.positions.push_back(position);
    }

    int normalCount = reader.ReadUint16();
    for (int normalNumber = 0; normalNumber < normalCount; ++normalNumber)
        shape.normals.push_back(ReadFixedVector(reader, NormalUnit));

    bool hasTextureCoordinates = HasTextureCoordinates(part);
    if (hasTextureCoordinates)
    {
        int textureCoordinateCount = reader.ReadUint16();
        for (int coordinateNumber = 0; coordinateNumber < textureCoordinateCount; ++coordinateNumber)
        {
            float s = reader.ReadInt16() / TextureCoordinateUnit;
            float t = reader.ReadInt16() / TextureCoordinateUnit;
            shape.textureCoordinates.push_back({s, t});
        }
    }

    ReadPrimitives(reader, shape, hasTextureCoordinates);
    return shape;
}

MiiTexture MiiResources::ReadTexture(TexturePart part, int entry) const
{
    Formats::ByteView entryBytes = EntryBytes(SectionOf(part), entry);
    Formats::ByteReader reader(entryBytes, std::format("Mii texture of section {} entry {}", SectionOf(part), entry));

    reader.MoveTo(TextureFormatOffset);
    std::uint32_t formatNumber = reader.ReadByte();
    reader.MoveTo(TextureWidthOffset);
    std::uint32_t width = reader.ReadUint16();
    reader.MoveTo(TextureHeightOffset);
    std::uint32_t height = reader.ReadUint16();
    reader.MoveTo(TextureHorizontalWrapOffset);
    std::uint32_t horizontalWrap = reader.ReadByte();
    reader.MoveTo(TextureVerticalWrapOffset);
    std::uint32_t verticalWrap = reader.ReadByte();
    reader.MoveTo(TextureDataOffsetOffset);
    std::uint32_t dataOffset = reader.ReadUint32();

    std::optional<Formats::TextureFormat> format = Formats::TextureFormatFromNumber(formatNumber);
    if (!format || Formats::IsPaletteFormat(*format))
        reader.Fail(std::format("the texture format {} is not one the face library uses", formatNumber));

    reader.MoveTo(dataOffset);
    Formats::ByteView pixels = reader.ReadBytes(Formats::EncodedImageSize(*format, width, height));

    MiiTexture texture;
    texture.image = Formats::DecodeTexture(*format, width, height, pixels, std::nullopt);
    texture.horizontalWrap = ToWrap(horizontalWrap, reader);
    texture.verticalWrap = ToWrap(verticalWrap, reader);
    return texture;
}

int MiiResources::EntryCount(ShapePart part) const
{
    return static_cast<int>(sections[SectionOf(part)].entryCount);
}

int MiiResources::EntryCount(TexturePart part) const
{
    return static_cast<int>(sections[SectionOf(part)].entryCount);
}

Formats::ByteView MiiResources::EntryBytes(int sectionNumber, int entry) const
{
    const Section& section = sections[sectionNumber];
    if (entry < 0 || static_cast<std::size_t>(entry) >= section.entryCount)
    {
        throw Formats::FormatError(
            std::format("Mii resources: section {} has {} entries, there is no entry {}", sectionNumber, section.entryCount, entry));
    }

    std::size_t start = section.entryOffsets[entry];
    std::size_t end = section.entryOffsets[entry + 1];
    if (end < start)
        throw Formats::FormatError(std::format("Mii resources: entry {} of section {} has a negative size", entry, sectionNumber));

    return Formats::ByteView(bytes).subspan(start, end - start);
}

}
