#include "Mii/MiiBody.h"

#include "Mii/MiiColors.h"

#include "Formats/FormatError.h"
#include "Graphics/Matrix4.h"

#include <cmath>
#include <map>
#include <set>
#include <string>

namespace Mii
{

namespace
{

using Graphics::Matrix4;
using Graphics::TevAlphaInput;
using Graphics::TevColorInput;

// The bones the Mii Channel treats in their own way while it sizes a body.
constexpr std::string_view SlopeBone = "aslope_root";
constexpr std::string_view PositionBone = "position_root";
constexpr std::string_view HeadBone = "head";
const std::set<std::string_view> UnsizedBones = {"all_root", "aslope_root", "position_root", "shadow_model"};
const std::set<std::string_view> WristAndAnkleBones = {"wrist_l", "wrist_r", "ankle_l", "ankle_r"};

// Height and weight run from 0 to 128.
constexpr float SizeUnit = 128.0f;
constexpr float SmallestLength = 0.5f;
constexpr float LengthPerHeight = 0.77f;
constexpr float SmallestThickness = 0.4f;
constexpr float ThicknessPerHeight = 0.23f;
constexpr float ThicknessPerWeight = 0.4f;
constexpr float ThicknessPerWeightAndHeight = 0.47f;

// A longer or thicker body lifts the whole Mii so that the feet stay on the ground.
constexpr float LiftPerLength = 60.0f;
constexpr float LiftPerThickness = 12.0f;

// The lit sphere is read with the direction of the surface: s = 0.5 + 0.5 x, t = 0.5 - 0.5 y.
constexpr Graphics::TextureMatrix SphereTextureMatrix = {{{{0.5f, 0.0f, 0.5f}, {0.0f, -0.5f, 0.5f}}}};

constexpr Formats::Rgba8Color OwnPants = {64, 70, 78, 255};
constexpr Formats::Rgba8Color FavoritePants = {112, 32, 16, 255};
constexpr Formats::Rgba8Color ForeignPants = {40, 64, 120, 255};
constexpr Formats::Rgba8Color SpecialPants = {192, 160, 48, 255};

struct BodySize
{
    float length;
    float thickness;
};

BodySize SizeOf(const MiiRecord& record)
{
    float height = static_cast<float>(record.height) / SizeUnit;
    float weight = static_cast<float>(record.weight) / SizeUnit;

    BodySize size;
    size.length = SmallestLength + LengthPerHeight * height;
    size.thickness = SmallestThickness + ThicknessPerHeight * height + weight * (ThicknessPerWeight + ThicknessPerWeightAndHeight * height);
    return size;
}

// The standing pose of a bone, upright (without the backward lean that the plaza's camera wants) and sized to the Mii.
BoneTransform StandingTransform(const MiiBodyFile& file, const BodyBone& bone, BodySize size)
{
    BoneTransform transform = bone.restTransform;
    auto posed = file.standingPose.find(bone.name);
    if (posed != file.standingPose.end())
        transform = posed->second;

    if (bone.name == SlopeBone)
        transform.rotationDegrees = {0, 0, 0};

    if (bone.name == PositionBone)
        transform.translation.y += LiftPerLength * (size.length - 1) + LiftPerThickness * (size.thickness - 1);

    if (WristAndAnkleBones.contains(bone.name))
        transform.scale = {size.thickness, size.thickness, size.thickness};
    else if (!UnsizedBones.contains(bone.name))
        transform.scale = {size.length, size.thickness, size.thickness};

    return transform;
}

Matrix4 RotationOf(Formats::Vector3 degrees)
{
    return Graphics::MakeRotationAroundZ(degrees.z) * Graphics::MakeRotationAroundY(degrees.y) * Graphics::MakeRotationAroundX(degrees.x);
}

// A bone moves, turns and scales in its parent's space; a bone that undoes its parent's scale does so before it turns.
Matrix4 LocalMatrix(const BoneTransform& transform, Formats::Vector3 parentScale)
{
    Matrix4 translation = Graphics::MakeTranslation(transform.translation);
    Matrix4 rotationAndScale = RotationOf(transform.rotationDegrees) * Graphics::MakeScale(transform.scale);
    if (!transform.undoesParentScale)
        return translation * rotationAndScale;

    Formats::Vector3 inverseScale = {1 / parentScale.x, 1 / parentScale.y, 1 / parentScale.z};
    return translation * Graphics::MakeScale(inverseScale) * rotationAndScale;
}

std::vector<Graphics::MeshVertex> PoseCorners(const BodyPart& part, const std::map<int, Matrix4>& matrixOfId)
{
    std::map<int, Matrix4> normalMatrixOfId;
    for (const auto& [matrixId, matrix] : matrixOfId)
        normalMatrixOfId[matrixId] = Graphics::MakeNormalMatrix(matrix);

    std::vector<Graphics::MeshVertex> corners;
    for (const BodyCorner& corner : part.triangleCorners)
    {
        Formats::Vector3 normal = Graphics::TransformDirection(normalMatrixOfId.at(corner.matrixId), part.normals.at(corner.normal));
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);

        Graphics::MeshVertex vertex = {};
        vertex.position = Graphics::TransformPoint(matrixOfId.at(corner.matrixId), part.positions.at(corner.position));
        vertex.normal = {normal.x / length, normal.y / length, normal.z / length};
        corners.push_back(vertex);
    }
    return corners;
}

const MiiBodyFile& FileFor(const MiiBodyFiles& files, const MiiRecord& record)
{
    if (record.gender == Gender::Female)
        return files.female;

    return files.male;
}

}

PosedBody PoseStandingBody(const MiiBodyFile& file, const MiiRecord& record)
{
    BodySize size = SizeOf(record);

    std::vector<Matrix4> worldMatrices;
    std::vector<Formats::Vector3> usedScales;
    std::map<int, Matrix4> matrixOfId;
    PosedBody posed = {};

    for (const BodyBone& bone : file.bones)
    {
        BoneTransform transform = StandingTransform(file, bone, size);

        Matrix4 world;
        if (bone.parent < 0)
            world = LocalMatrix(transform, {1, 1, 1});
        else
            world = worldMatrices[bone.parent] * LocalMatrix(transform, usedScales[bone.parent]);

        worldMatrices.push_back(world);
        usedScales.push_back(transform.scale);
        matrixOfId[bone.matrixId] = world;

        if (bone.name == HeadBone)
            posed.headPosition = Graphics::TransformPoint(world, {0, 0, 0});
    }

    posed.shirt = PoseCorners(file.shirt, matrixOfId);
    posed.pants = PoseCorners(file.pants, matrixOfId);
    return posed;
}

Formats::Rgba8Color PantsColorFor(const MiiRecord& record, std::uint32_t consoleSystemId)
{
    if (IsSpecialMii(record))
        return SpecialPants;

    if (!WasMadeOnConsole(record, consoleSystemId))
        return ForeignPants;

    if (record.isFavorite)
        return FavoritePants;

    return OwnPants;
}

MiiBody::MiiBody(
    Graphics::GraphicsDevice& graphicsDevice, const MiiBodyFiles& files, const MiiRecord& record, std::uint32_t consoleSystemId)
    : device(graphicsDevice)
    , body(PoseStandingBody(FileFor(files, record), record))
    , shirtColor(FavoriteColors[record.favoriteColor])
    , pantsColor(PantsColorFor(record, consoleSystemId))
{
    const Formats::RgbaImage& sphere = FileFor(files, record).environmentTexture;

    Graphics::TextureDescription description;
    description.width = sphere.width;
    description.height = sphere.height;
    description.pixels = sphere.pixels;
    environmentTexture = device.CreateTexture(description);
}

MiiBody::~MiiBody()
{
    device.DestroyTexture(environmentTexture);
}

Formats::Vector3 MiiBody::HeadPosition() const
{
    return body.headPosition;
}

void MiiBody::Draw(const Graphics::MeshPlacement& placement) const
{
    DrawPart(body.shirt, placement, shirtColor);
    DrawPart(body.pants, placement, pantsColor);
}

// color = part color + sphere x sphere, limited to white.
void MiiBody::DrawPart(
    const std::vector<Graphics::MeshVertex>& corners, const Graphics::MeshPlacement& placement, Formats::Rgba8Color color) const
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.usesRasterColor = false;
    stage.color.inputB = TevColorInput::TextureColor;
    stage.color.inputC = TevColorInput::TextureColor;
    stage.color.inputD = TevColorInput::Register0Color;
    stage.alpha.inputD = TevAlphaInput::Register0Alpha;

    Graphics::Material material;
    material.blendMode = Graphics::BlendMode::Over;
    material.textures = {{environmentTexture, SphereTextureMatrix}};
    material.colorRegisters[0] = {color.red, color.green, color.blue, color.alpha};
    material.stages = {stage};

    Graphics::MeshPlacement partPlacement = placement;
    partPlacement.light.reset();
    partPlacement.textureCoordinatesFromNormal = true;
    partPlacement.culling = Graphics::FaceCulling::Back;
    partPlacement.depth = Graphics::DepthMode::TestAndWrite;

    device.DrawMesh(corners, partPlacement, material);
}

}
