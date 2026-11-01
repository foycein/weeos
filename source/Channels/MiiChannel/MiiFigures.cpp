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

// A Mii in the hand is lifted above the ground (Assumed, CHN-66).
constexpr float HeldLift = 80.0f;

// The shadows lie a unit above the ground, so that the ground's depth does not hide them (Assumed, CHN-80). A held Mii's shadow stays
// on the ground.
constexpr float ShadowLift = 1.0f;

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
// (Assumed, CHN-71).
constexpr Formats::Vector3 GroundPlace = {0.0f, 0.0f, -2700.0f};
constexpr std::uint32_t FaceTextureSize = 128;

// The faces of Mii::MiiExpression, from Normal to OpenMouth.
constexpr std::size_t ExpressionCount = 7;

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

// Where the body of a plaza Mii stands; a held Mii hangs above the ground.
Graphics::Matrix4 PlazaPlaceOf(const PlazaMii& plazaMii)
{
    float lift = plazaMii.activity == PlazaActivity::Held ? HeldLift : 0.0f;
    return Graphics::MakeTranslation({plazaMii.position.x, lift, plazaMii.position.y});
}

}

std::size_t PlazaHeadKey(std::size_t slot, Mii::MiiExpression expression)
{
    return slot * ExpressionCount + static_cast<std::size_t>(expression);
}

// Every Mii keeps its normal face, and while it talks or sleeps the face of that too, so that its mouth and eyes change without waiting.
std::vector<ShownHead> HeadsOfMiis(const MiiPlaza& plaza)
{
    std::vector<ShownHead> shownHeads;
    for (const PlazaMii& plazaMii : plaza.Miis())
    {
        std::size_t slot = plazaMii.mii.slot;
        shownHeads.push_back({PlazaHeadKey(slot, Mii::MiiExpression::Normal), plazaMii.mii.record, Mii::MiiExpression::Normal});

        bool talks = plazaMii.activity == PlazaActivity::Talking || plazaMii.activity == PlazaActivity::Answering;
        Mii::MiiExpression face = ExpressionOf(plazaMii);
        if (talks)
            face = Mii::MiiExpression::OpenMouth;
        if (face != Mii::MiiExpression::Normal)
            shownHeads.push_back({PlazaHeadKey(slot, face), plazaMii.mii.record, face});
    }
    return shownHeads;
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

void MiiHeads::Prepare(const std::vector<ShownHead>& miis)
{
    std::map<std::size_t, KeptHead> keptHeads;
    for (const auto& [key, record, expression] : miis)
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
        kept.head = std::make_unique<Mii::MiiHead>(device, parts, record, expression, FaceTextureSize);
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
        Graphics::MeshPlacement shadowPlacement;
        shadowPlacement.modelView = view * Graphics::MakeTranslation({plazaMii.position.x, ShadowLift, plazaMii.position.y});
        shadowPlacement.projection = projection;
        bodyModels.DrawShadow(plazaMii.mii.record.gender, shadowPlacement);
    }

    for (const PlazaMii& plazaMii : plaza.Miis())
    {
        const Mii::MiiHead* head = heads.Find(PlazaHeadKey(plazaMii.mii.slot, ExpressionOf(plazaMii)));
        if (head == nullptr)
            continue;

        DrawFigure(plazaMii.mii.record, *head, PlazaStanceOf(plazaMii), view * PlazaPlaceOf(plazaMii), projection);
    }
}

Formats::Vector3 MiiFigures::HeadPositionOf(const PlazaMii& plazaMii) const
{
    const Mii::MiiRecord& record = plazaMii.mii.record;
    Mii::BodyPose pose = Mii::PoseBody(BodyFileOf(bodies, record), record, PlazaStanceOf(plazaMii));
    return Graphics::TransformPoint(PlazaPlaceOf(plazaMii) * Mii::HeadMatrixOf(pose), {0, 0, 0});
}

Mii::BodyStance MiiFigures::PlazaStanceOf(const PlazaMii& plazaMii) const
{
    Mii::BodyStance stance;
    stance.animation = &BodyFileOf(bodies, plazaMii.mii.record).animations.at(std::string(AnimationOf(plazaMii.activity)));
    stance.frame = plazaMii.animationFrame;
    stance.facingDegrees = plazaMii.facingDegrees;
    stance.leansBack = true;
    return stance;
}

void MiiFigures::DrawStandingMii(const Mii::MiiRecord& record, const Mii::MiiHead& head, const PictureRectangle& rectangle,
    Graphics::Canvas canvas, float waitFrame) const
{
    Mii::BodyStance stance;
    stance.animation = &BodyFileOf(bodies, record).animations.at(std::string(StandingAnimation));
    stance.frame = waitFrame;
    DrawPosedMii(record, head, stance, rectangle, canvas);
}

void MiiFigures::DrawPosedMii(const Mii::MiiRecord& record, const Mii::MiiHead& head, const Mii::BodyStance& stance,
    const PictureRectangle& rectangle, Graphics::Canvas canvas) const
{
    FigureCamera camera = StandingCamera(rectangle, canvas);
    DrawFigure(record, head, stance, camera.view, camera.projection);
}

FigureCamera MiiFigures::StandingCamera(const PictureRectangle& rectangle, Graphics::Canvas canvas) const
{
    float aspect = rectangle.halfSize.x / rectangle.halfSize.y;
    Graphics::Matrix4 projection = Graphics::MakePerspective(StandingViewDegrees, aspect, StandingNear, StandingFar);
    return {Graphics::MakeLookAt(StandingEye, StandingTarget, Upward), ProjectIntoRectangle(projection, rectangle, canvas)};
}

Formats::Vector3 MiiFigures::StandingHeadPosition(const Mii::MiiRecord& record) const
{
    return Mii::PoseStandingBody(BodyFileOf(bodies, record), record).headPosition;
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
    headPlacement.modelView = modelView * Mii::HeadMatrixOf(pose);
    headPlacement.light = Mii::MakeHeadLight(headPlacement.modelView);
    head.Draw(headPlacement);
}

}
