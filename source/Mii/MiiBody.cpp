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

// Register 0 of the material mt_shadow, the color of the shadow; its alpha is the brightness of the picture "shadow".
constexpr Formats::Rgba8Color ShadowColor = {32, 40, 38, 255};

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
        float lengthLift = LiftPerLength * (size.length - 1);
        float thicknessLift = LiftPerThickness * (size.thickness - 1);
        transform.translation.y += lengthLift + thicknessLift;
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

Formats::Vector3 ColumnOf(const Matrix4& matrix, std::size_t column)
{
    return {matrix.rows[0][column], matrix.rows[1][column], matrix.rows[2][column]};
}

Formats::Vector3 Normalized(Formats::Vector3 vector)
{
    float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    return {vector.x / length, vector.y / length, vector.z / length};
}

Formats::Vector3 Cross(Formats::Vector3 first, Formats::Vector3 second)
{
    return {first.y * second.z - first.z * second.y, first.z * second.x - first.x * second.z, first.x * second.y - first.y * second.x};
}

Graphics::TextureHandle UploadPicture(Graphics::GraphicsDevice& device, const Formats::RgbaImage& picture)
{
    Graphics::TextureDescription description;
    description.width = picture.width;
    description.height = picture.height;
    description.pixels = picture.pixels;
    return device.CreateTexture(description);
}

// The shadow's normals point up; nothing reads them, as the shadow has no light.
std::vector<Graphics::MeshVertex> ShadowCorners(const BodyShadow& shadow)
{
    std::vector<Graphics::MeshVertex> corners;
    for (std::size_t cornerNumber = 0; cornerNumber < shadow.cornerPositions.size(); ++cornerNumber)
    {
        Graphics::MeshVertex vertex = {};
        vertex.position = shadow.cornerPositions[cornerNumber];
        vertex.normal = {0, 1, 0};
        vertex.textureCoordinate = shadow.cornerTextureCoordinates[cornerNumber];
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
    float thicknessFromWeight = weight * (ThicknessPerWeight + ThicknessPerWeightAndHeight * height);
    size.thickness = SmallestThickness + ThicknessPerHeight * height + thicknessFromWeight;
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
        {
            pose.headPosition = Graphics::TransformPoint(world, {0, 0, 0});
            pose.headBoneMatrix = world;
        }
    }
    return pose;
}

Matrix4 HeadMatrixOf(const BodyPose& pose)
{
    Formats::Vector3 up = Normalized(ColumnOf(pose.headBoneMatrix, 0));
    Formats::Vector3 front = Normalized(ColumnOf(pose.headBoneMatrix, 1));
    Formats::Vector3 right = Cross(up, front);

    Matrix4 head;
    head.rows[0] = {right.x, up.x, front.x, pose.headPosition.x};
    head.rows[1] = {right.y, up.y, front.y, pose.headPosition.y};
    head.rows[2] = {right.z, up.z, front.z, pose.headPosition.z};
    return head;
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
    environmentTexture = UploadPicture(device, bodyFiles.male.environmentTexture);
    shadowTexture = UploadPicture(device, bodyFiles.male.shadow.picture);
}

MiiBodyModels::~MiiBodyModels()
{
    device.DestroyTexture(environmentTexture);
    device.DestroyTexture(shadowTexture);
    for (const GenderModel& model : {male, female})
    {
        device.DestroyMesh(model.shirt);
        device.DestroyMesh(model.pants);
        device.DestroyMesh(model.shadow);
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
    model.shadow = device.CreateMesh(ShadowCorners(file.shadow));
    return model;
}

const MiiBodyModels::GenderModel& MiiBodyModels::ModelFor(Gender gender) const
{
    if (gender == Gender::Female)
        return female;

    return male;
}

void MiiBodyModels::DrawShadow(Gender gender, const Graphics::MeshPlacement& placement) const
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.usesRasterColor = false;
    stage.color.inputD = TevColorInput::Register0Color;
    stage.alpha.inputD = TevAlphaInput::TextureAlpha;

    Graphics::Material material;
    material.blendMode = Graphics::BlendMode::StandardAlpha;
    material.textures = {{shadowTexture, {}}};
    material.colorRegisters[0] = {ShadowColor.red, ShadowColor.green, ShadowColor.blue, ShadowColor.alpha};
    material.stages = {stage};

    Graphics::MeshPlacement shadowPlacement = placement;
    shadowPlacement.light.reset();
    shadowPlacement.culling = Graphics::FaceCulling::None;
    shadowPlacement.depth = Graphics::DepthMode::TestOnly;
    device.DrawMesh(ModelFor(gender).shadow, shadowPlacement, material);
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
