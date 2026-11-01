#include "Mii/MiiHeadModel.h"

#include <cmath>
#include <cstdint>

namespace Mii
{

namespace
{

constexpr float FixedPointUnit = 256.0f;
constexpr int FixedPointShift = 8;

constexpr float NoseScalePerSize = 0.175f;
constexpr float SmallestNoseScale = 0.4f;
constexpr int MiddleNoseVertical = 8;
constexpr float GlassesScalePerSize = 0.15f;
constexpr float SmallestGlassesScale = 0.4f;
constexpr int MiddleGlassesVertical = 11;
constexpr float GlassesRiseOverNose = 5.0f;
constexpr float GlassesDistanceBeforeNose = 2.0f;
constexpr float DropPerVerticalStep = 1.5f;

constexpr int GlassesShapeEntry = 0;

// How a part is put on the face. The face library does it in whole 1/256 units: the scale and the move are cut to
// 1/256 first, and every position is scaled and moved in whole numbers.
struct PartPlacement
{
    bool isMoved = false;
    float scale = 1.0f;
    Formats::Vector3 offset = {0, 0, 0};
    bool isMirrored = false;
};

int ToFixedPoint(float value)
{
    return static_cast<int>(FixedPointUnit * value);
}

float PlaceCoordinate(int stored, int scale, int offset)
{
    auto placed = static_cast<std::int16_t>(offset + ((stored * scale) >> FixedPointShift));
    return static_cast<float>(placed) / FixedPointUnit;
}

std::vector<Graphics::MeshVertex> MakeCorners(const MiiShape& shape, const PartPlacement& placement)
{
    int scale = ToFixedPoint(placement.scale);
    int offsetX = ToFixedPoint(placement.offset.x);
    int offsetY = ToFixedPoint(placement.offset.y);
    int offsetZ = ToFixedPoint(placement.offset.z);

    std::vector<Graphics::MeshVertex> corners;
    for (const ShapeCorner& corner : shape.triangleCorners)
    {
        const StoredPosition& stored = shape.positions[corner.position];
        int x = stored.x;
        if (placement.isMirrored)
            x = -x;

        Graphics::MeshVertex vertex = {};
        vertex.position = {x / FixedPointUnit, stored.y / FixedPointUnit, stored.z / FixedPointUnit};
        if (placement.isMoved)
        {
            vertex.position.x = PlaceCoordinate(x, scale, offsetX);
            vertex.position.y = PlaceCoordinate(stored.y, scale, offsetY);
            vertex.position.z = PlaceCoordinate(stored.z, scale, offsetZ);
        }

        vertex.normal = shape.normals[corner.normal];
        if (placement.isMirrored)
            vertex.normal.x = -vertex.normal.x;

        if (!shape.textureCoordinates.empty())
            vertex.textureCoordinate = shape.textureCoordinates[corner.textureCoordinate];

        corners.push_back(vertex);
    }
    return corners;
}

std::vector<Graphics::MeshVertex> BuildPart(const MiiResources& resources, ShapePart part, int entry, const PartPlacement& placement)
{
    return MakeCorners(resources.ReadShape(part, entry), placement);
}

PartPlacement MovedBy(Formats::Vector3 offset, bool isMirrored)
{
    PartPlacement placement;
    placement.isMoved = true;
    placement.offset = offset;
    placement.isMirrored = isMirrored;
    return placement;
}

// The original works these out in single precision with one rounding (a multiply and add in one step).
PartPlacement NosePlacement(const MiiNose& nose, Formats::Vector3 noseAnchor)
{
    PartPlacement placement;
    placement.isMoved = true;
    placement.scale = std::fma(NoseScalePerSize, static_cast<float>(nose.size), SmallestNoseScale);
    placement.offset = noseAnchor;
    placement.offset.y = std::fma(-DropPerVerticalStep, static_cast<float>(nose.vertical - MiddleNoseVertical), noseAnchor.y);
    return placement;
}

PartPlacement GlassesPlacement(const MiiGlasses& glasses, Formats::Vector3 noseAnchor)
{
    PartPlacement placement;
    placement.isMoved = true;
    placement.scale = std::fma(GlassesScalePerSize, static_cast<float>(glasses.size), SmallestGlassesScale);
    placement.offset = noseAnchor;
    placement.offset.y =
        std::fma(-DropPerVerticalStep, static_cast<float>(glasses.vertical - MiddleGlassesVertical), noseAnchor.y + GlassesRiseOverNose);
    placement.offset.z = noseAnchor.z + GlassesDistanceBeforeNose;
    return placement;
}

}

MiiHeadModel BuildMiiHead(const MiiResources& resources, const MiiRecord& record)
{
    MiiShape faceline = resources.ReadShape(ShapePart::Faceline, record.face.shape);
    Formats::Vector3 noseAnchor = faceline.anchorPoints[NoseAnchor];
    Formats::Vector3 beardAnchor = faceline.anchorPoints[BeardAnchor];
    Formats::Vector3 hairAnchor = faceline.anchorPoints[HairAnchor];
    bool isMirrored = record.hair.isMirrored;

    MiiHeadModel model;
    model.faceline = MakeCorners(faceline, PartPlacement());
    model.cap = BuildPart(resources, ShapePart::Cap, record.hair.type, MovedBy(hairAnchor, isMirrored));
    model.hair = BuildPart(resources, ShapePart::Hair, record.hair.type, MovedBy(hairAnchor, isMirrored));
    model.forehead = BuildPart(resources, ShapePart::Forehead, record.hair.type, MovedBy(hairAnchor, isMirrored));
    model.beard = BuildPart(resources, ShapePart::Beard, record.facialHair.beardType, MovedBy(beardAnchor, false));
    model.nose = BuildPart(resources, ShapePart::Nose, record.nose.type, NosePlacement(record.nose, noseAnchor));
    model.noseline = BuildPart(resources, ShapePart::Noseline, record.nose.type, NosePlacement(record.nose, noseAnchor));
    model.mask = BuildPart(resources, ShapePart::Mask, record.face.shape, PartPlacement());
    model.glasses = BuildPart(resources, ShapePart::Glasses, GlassesShapeEntry, GlassesPlacement(record.glasses, noseAnchor));
    model.hairIsMirrored = isMirrored;
    return model;
}

}
