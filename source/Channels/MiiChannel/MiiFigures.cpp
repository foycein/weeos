#include "Channels/MiiChannel/MiiFigures.h"

#include "Mii/MiiRecord.h"

#include <array>
#include <cmath>
#include <numbers>

namespace Channels
{

namespace
{

constexpr std::string_view StandingAnimation = "bodyWait";
constexpr std::string_view WalkingAnimation = "bodyWalk";

// A Mii in the hand dangles and kicks (bodyWallow, 12 frames), lifted above the ground (Assumed, CHN-38).
constexpr std::string_view HeldAnimation = "bodyWallow";
constexpr float HeldLift = 80.0f;

// Assumed (MII-20): the camera of the editor's Mii looks at it from the front and shows the tallest one whole.
constexpr Formats::Vector3 StandingEye = {0.0f, 110.0f, 700.0f};
constexpr Formats::Vector3 StandingTarget = {0.0f, 110.0f, 0.0f};
constexpr float StandingViewDegrees = 19.5f;
constexpr float StandingNear = 100.0f;
constexpr float StandingFar = 2000.0f;

// The camera of the face library's icons (docs/research/Mii.md 9.2): 600 units in front of the face, 9.88 degrees high.
constexpr Formats::Vector3 HeadEye = {0.0f, 34.5f, 600.0f};
constexpr Formats::Vector3 HeadTarget = {0.0f, 34.5f, 0.0f};
constexpr float HeadViewDegrees = 9.88f;
constexpr float HeadNear = 500.0f;
constexpr float HeadFar = 700.0f;

constexpr Formats::Vector3 Upward = {0.0f, 1.0f, 0.0f};

// The ground reaches from 0 to 5400 in depth with the middle of its paving at 2700; it lies with that middle under the middle of the plaza
// (Assumed, CHN-43).
constexpr Formats::Vector3 GroundPlace = {0.0f, 0.0f, -2700.0f};
constexpr std::uint32_t FaceTextureSize = 128;

std::vector<std::uint8_t> StoredBytes(const Mii::MiiRecord& record)
{
    std::array<Formats::Byte, Mii::MiiRecordSize> bytes = {};
    Mii::WriteMiiRecord(record, bytes);
    return {bytes.begin(), bytes.end()};
}

const Mii::MiiBodyFile& BodyFileOf(const Mii::MiiBodyFiles& bodies, const Mii::MiiRecord& record)
{
    if (record.gender == Mii::Gender::Female)
        return bodies.female;

    return bodies.male;
}

std::string_view AnimationOf(PlazaActivity activity)
{
    switch (activity)
    {
    case PlazaActivity::Walking:
        return WalkingAnimation;
    case PlazaActivity::Held:
        return HeldAnimation;
    case PlazaActivity::Waiting:
        return StandingAnimation;
    }
    return StandingAnimation;
}

}

Graphics::Matrix4 ProjectIntoRectangle(const Graphics::Matrix4& projection, const PictureRectangle& rectangle, Graphics::Canvas canvas)
{
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;

    Graphics::Matrix4 intoRectangle;
    intoRectangle.rows[0] = {rectangle.halfSize.x / halfWidth, 0, 0, rectangle.middle.x / halfWidth};
    intoRectangle.rows[1] = {0, rectangle.halfSize.y / halfHeight, 0, rectangle.middle.y / halfHeight};
    return intoRectangle * projection;
}

MiiHeads::MiiHeads(Graphics::GraphicsDevice& graphicsDevice, const Mii::MiiResources& faceParts)
    : device(graphicsDevice)
    , parts(faceParts)
{
}

void MiiHeads::Prepare(const std::vector<std::pair<std::size_t, Mii::MiiRecord>>& miis)
{
    std::map<std::size_t, KeptHead> keptHeads;
    for (const auto& [key, record] : miis)
    {
        std::vector<std::uint8_t> bytes = StoredBytes(record);
        auto existing = heads.find(key);
        if (existing != heads.end() && existing->second.recordBytes == bytes)
        {
            keptHeads[key] = std::move(existing->second);
            continue;
        }

        KeptHead kept;
        kept.recordBytes = bytes;
        kept.head = std::make_unique<Mii::MiiHead>(device, parts, record, Mii::MiiExpression::Normal, FaceTextureSize);
        keptHeads[key] = std::move(kept);
    }
    heads = std::move(keptHeads);
}

const Mii::MiiHead* MiiHeads::Find(std::size_t key) const
{
    auto found = heads.find(key);
    if (found == heads.end())
        return nullptr;

    return found->second.head.get();
}

MiiFigures::MiiFigures(Graphics::GraphicsDevice& graphicsDevice, const Mii::MiiBodyFiles& bodyFiles, std::uint32_t consoleSystemId)
    : device(graphicsDevice)
    , bodies(bodyFiles)
    , bodyModels(graphicsDevice, bodyFiles)
    , systemId(consoleSystemId)
{
}

void MiiFigures::DrawPlaza(const MiiPlaza& plaza, const PlazaGround& ground, const MiiHeads& heads) const
{
    Graphics::Matrix4 view = plaza.ViewMatrix();
    Graphics::Matrix4 projection = plaza.ProjectionMatrix();
    ground.Draw(view * Graphics::MakeTranslation(GroundPlace), projection);

    for (const PlazaMii& plazaMii : plaza.Miis())
    {
        const Mii::MiiHead* head = heads.Find(plazaMii.mii.slot);
        if (head == nullptr)
            continue;

        const Mii::MiiRecord& record = plazaMii.mii.record;
        Mii::BodyStance stance;
        stance.animation = &BodyFileOf(bodies, record).animations.at(std::string(AnimationOf(plazaMii.activity)));
        stance.frame = plazaMii.animationFrame;
        stance.facingDegrees = plazaMii.facingDegrees;
        stance.leansBack = true;

        float lift = plazaMii.activity == PlazaActivity::Held ? HeldLift : 0.0f;
        Graphics::Matrix4 place = Graphics::MakeTranslation({plazaMii.position.x, lift, plazaMii.position.y});
        DrawFigure(record, *head, stance, view * place, projection);
    }
}

void MiiFigures::DrawStandingMii(const Mii::MiiRecord& record, const Mii::MiiHead& head, const PictureRectangle& rectangle,
    Graphics::Canvas canvas, float waitFrame) const
{
    Mii::BodyStance stance;
    stance.animation = &BodyFileOf(bodies, record).animations.at(std::string(StandingAnimation));
    stance.frame = waitFrame;

    float aspect = rectangle.halfSize.x / rectangle.halfSize.y;
    Graphics::Matrix4 projection = Graphics::MakePerspective(StandingViewDegrees, aspect, StandingNear, StandingFar);
    Graphics::Matrix4 view = Graphics::MakeLookAt(StandingEye, StandingTarget, Upward);
    DrawFigure(record, head, stance, view, ProjectIntoRectangle(projection, rectangle, canvas));
}

void MiiFigures::DrawHead(const Mii::MiiHead& head, const PictureRectangle& rectangle, Graphics::Canvas canvas) const
{
    float aspect = rectangle.halfSize.x / rectangle.halfSize.y;
    Graphics::Matrix4 projection = Graphics::MakePerspective(HeadViewDegrees, aspect, HeadNear, HeadFar);

    Graphics::MeshPlacement placement;
    placement.modelView = Graphics::MakeLookAt(HeadEye, HeadTarget, Upward);
    placement.projection = ProjectIntoRectangle(projection, rectangle, canvas);
    placement.light = Mii::MakeHeadLight(placement.modelView);
    head.Draw(placement);
}

const Mii::MiiBodyFiles& MiiFigures::Bodies() const
{
    return bodies;
}

void MiiFigures::DrawFigure(const Mii::MiiRecord& record, const Mii::MiiHead& head, const Mii::BodyStance& stance,
    const Graphics::Matrix4& modelView, const Graphics::Matrix4& projection) const
{
    Mii::BodyPose pose = Mii::PoseBody(BodyFileOf(bodies, record), record, stance);

    Graphics::MeshPlacement bodyPlacement;
    bodyPlacement.modelView = modelView;
    bodyPlacement.projection = projection;
    bodyModels.Draw(record, systemId, pose, bodyPlacement);

    Graphics::MeshPlacement headPlacement = bodyPlacement;
    headPlacement.modelView = modelView * Mii::HeadMatrixOf(pose, stance);
    headPlacement.light = Mii::MakeHeadLight(headPlacement.modelView);
    head.Draw(headPlacement);
}

}
