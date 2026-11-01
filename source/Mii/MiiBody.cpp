#include "Mii/MiiBody.h"

#include "Mii/MiiColors.h"

#include <cmath>
#include <set>
#include <string>

namespace Mii
{

namespace
{

using Graphics::Matrix4;
using Graphics::TevAlphaInput;
using Graphics::TevColorInput;

// The bones the Mii Channel treats in their own way while it sizes and turns a body.
constexpr std::string_view SlopeBone = "aslope_root";
constexpr std::string_view PositionBone = "position_root";
constexpr std::string_view HeadBone = "head";
const std::set<std::string_view> UnsizedBones = {"all_root", "aslope_root", "position_root", "shadow_model"};
const std::set<std::string_view> WristAndAnkleBones = {"wrist_l", "wrist_r", "ankle_l", "ankle_r"};

// The lean of aslope_root in the animations of the plaza, about x.
constexpr float PlazaLeanDegrees = -45.0f;

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

// The transform of a bone in the stance: the animation's (bodyWait frame 0 without one), without the lean when the body stands upright,
// turned toward the facing direction and sized to the Mii.
BoneTransform PosedTransform(const MiiBodyFile& file, const BodyBone& bone, BodySize size, const BodyStance& stance)
{
    BoneTransform transform = bone.restTransform;
    if (stance.animation != nullptr)
    {
        auto animated = stance.animation->bones.find(bone.name);
        if (animated != stance.animation->bones.end())
            transform = AnimatedTransform(animated->second, BoneTransform(), stance.frame);
    }
    else
    {
        auto posed = file.standingPose.find(bone.name);
        if (posed != file.standingPose.end())
            transform = posed->second;
    }

    if (bone.name == SlopeBone && !stance.leansBack)
        transform.rotationDegrees = {0, 0, 0};

    if (bone.name == PositionBone)
    {
        transform.translation.y += LiftPerLength * (size.length - 1) + LiftPerThickness * (size.thickness - 1);
        transform.rotationDegrees.y += stance.facingDegrees;
    }

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

std::vector<Graphics::MeshVertex> PoseCorners(const BodyPart& part, const std::vector<Matrix4>& boneMatrices)
{
    std::vector<Matrix4> normalMatrices;
    for (const Matrix4& boneMatrix : boneMatrices)
        normalMatrices.push_back(Graphics::MakeNormalMatrix(boneMatrix));

    std::vector<Graphics::MeshVertex> corners;
    for (const BodyCorner& corner : part.triangleCorners)
    {
        Formats::Vector3 normal = Graphics::TransformDirection(normalMatrices.at(corner.matrixId), part.normals.at(corner.normal));
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);

        Graphics::MeshVertex vertex = {};
        vertex.position = Graphics::TransformPoint(boneMatrices.at(corner.matrixId), part.positions.at(corner.position));
        vertex.normal = {normal.x / length, normal.y / length, normal.z / length};
        corners.push_back(vertex);
    }
    return corners;
}

// The corners in the space of their bones, each with the number of its bone, for the graphics card.
std::vector<Graphics::MeshVertex> CornersOnBones(const BodyPart& part)
{
    std::vector<Graphics::MeshVertex> corners;
    for (const BodyCorner& corner : part.triangleCorners)
    {
        Graphics::MeshVertex vertex = {};
        vertex.position = part.positions.at(corner.position);
        vertex.normal = part.normals.at(corner.normal);
        vertex.boneNumber = static_cast<std::uint32_t>(corner.matrixId);
        corners.push_back(vertex);
    }
    return corners;
}

}

BodySize SizeOf(const MiiRecord& record)
{
    float height = static_cast<float>(record.height) / SizeUnit;
    float weight = static_cast<float>(record.weight) / SizeUnit;

    BodySize size;
    size.length = SmallestLength + LengthPerHeight * height;
    size.thickness = SmallestThickness + ThicknessPerHeight * height + weight * (ThicknessPerWeight + ThicknessPerWeightAndHeight * height);
    return size;
}

BodyPose PoseBody(const MiiBodyFile& file, const MiiRecord& record, const BodyStance& stance)
{
    BodySize size = SizeOf(record);

    std::vector<Matrix4> worldMatrices;
    std::vector<Formats::Vector3> usedScales;
    BodyPose pose;

    for (const BodyBone& bone : file.bones)
    {
        BoneTransform transform = PosedTransform(file, bone, size, stance);

        Matrix4 world;
        if (bone.parent < 0)
            world = LocalMatrix(transform, {1, 1, 1});
        else
            world = worldMatrices[bone.parent] * LocalMatrix(transform, usedScales[bone.parent]);

        worldMatrices.push_back(world);
        usedScales.push_back(transform.scale);

        auto matrixId = static_cast<std::size_t>(bone.matrixId);
        if (pose.boneMatrices.size() <= matrixId)
            pose.boneMatrices.resize(matrixId + 1);
        pose.boneMatrices[matrixId] = world;

        if (bone.name == HeadBone)
            pose.headPosition = Graphics::TransformPoint(world, {0, 0, 0});
    }
    return pose;
}

Matrix4 HeadMatrixOf(const BodyPose& pose, const BodyStance& stance)
{
    Matrix4 lean;
    if (stance.leansBack)
        lean = Graphics::MakeRotationAroundX(PlazaLeanDegrees);

    return Graphics::MakeTranslation(pose.headPosition) * lean * Graphics::MakeRotationAroundY(stance.facingDegrees);
}

PosedBody PoseStandingBody(const MiiBodyFile& file, const MiiRecord& record)
{
    BodyPose pose = PoseBody(file, record, BodyStance());

    PosedBody posed;
    posed.shirt = PoseCorners(file.shirt, pose.boneMatrices);
    posed.pants = PoseCorners(file.pants, pose.boneMatrices);
    posed.headPosition = pose.headPosition;
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

MiiBodyModels::MiiBodyModels(Graphics::GraphicsDevice& graphicsDevice, const MiiBodyFiles& bodyFiles)
    : device(graphicsDevice)
    , files(bodyFiles)
    , male(MakeModel(bodyFiles.male))
    , female(MakeModel(bodyFiles.female))
{
    const Formats::RgbaImage& sphere = bodyFiles.male.environmentTexture;

    Graphics::TextureDescription description;
    description.width = sphere.width;
    description.height = sphere.height;
    description.pixels = sphere.pixels;
    environmentTexture = device.CreateTexture(description);
}

MiiBodyModels::~MiiBodyModels()
{
    device.DestroyTexture(environmentTexture);
    for (const GenderModel& model : {male, female})
    {
        device.DestroyMesh(model.shirt);
        device.DestroyMesh(model.pants);
    }
}

const MiiBodyFile& MiiBodyModels::FileFor(Gender gender) const
{
    if (gender == Gender::Female)
        return files.female;

    return files.male;
}

void MiiBodyModels::Draw(const MiiRecord& record, std::uint32_t consoleSystemId, const BodyPose& pose,
    const Graphics::MeshPlacement& placement) const
{
    const GenderModel& model = ModelFor(record.gender);
    DrawPart(model.shirt, pose, placement, FavoriteColors[record.favoriteColor]);
    DrawPart(model.pants, pose, placement, PantsColorFor(record, consoleSystemId));
}

MiiBodyModels::GenderModel MiiBodyModels::MakeModel(const MiiBodyFile& file)
{
    GenderModel model;
    model.shirt = device.CreateMesh(CornersOnBones(file.shirt));
    model.pants = device.CreateMesh(CornersOnBones(file.pants));
    return model;
}

const MiiBodyModels::GenderModel& MiiBodyModels::ModelFor(Gender gender) const
{
    if (gender == Gender::Female)
        return female;

    return male;
}

// color = part color + sphere x sphere, limited to white.
void MiiBodyModels::DrawPart(Graphics::MeshHandle part, const BodyPose& pose, const Graphics::MeshPlacement& placement,
    Formats::Rgba8Color color) const
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
    partPlacement.boneMatrices = pose.boneMatrices;

    device.DrawMesh(part, partPlacement, material);
}

}
