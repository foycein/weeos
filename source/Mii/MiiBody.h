#pragma once

#include "Mii/MiiBodyFile.h"
#include "Mii/MiiRecord.h"

#include "Graphics/GraphicsDevice.h"
#include "Graphics/Matrix4.h"

#include <cstdint>
#include <vector>

namespace Mii
{

// How long and how thick the bones of a Mii are: height makes them longer, weight thicker (docs/research/Mii.md section 10.3).
struct BodySize
{
    float length = 1;
    float thickness = 1;
};

BodySize SizeOf(const MiiRecord& record);

// How a body stands: the animation and its frame, the direction it faces and whether it leans back for the high camera of the plaza.
// Without an animation the body stands in frame 0 of bodyWait.
struct BodyStance
{
    const BodyAnimation* animation = nullptr;
    float frame = 0;

    // A turn about the vertical axis, 0 looks toward +z, the front of the plaza.
    float facingDegrees = 0;

    // Every animation tilts the body back by 45 degrees (the bone aslope_root) toward the camera of the plaza, which looks down on it;
    // the editor shows the Mii upright.
    bool leansBack = false;
};

// Where the bones of a body are in one frame.
struct BodyPose
{
    // By matrix id: from the space of the bone into the space of the body, whose feet stand on y = 0 at the origin.
    std::vector<Graphics::Matrix4> boneMatrices;

    // Where the chin of the head goes (the head bone), in the space of the body.
    Formats::Vector3 headPosition;
};

BodyPose PoseBody(const MiiBodyFile& file, const MiiRecord& record, const BodyStance& stance);

// The matrix that places the head of a posed body: the head stands on the head bone in its own size, upright or leaning back with the
// body, and turned toward the facing direction (the Mii Channel removes the bone's scale and turn, docs/research/Mii.md 10.4).
Graphics::Matrix4 HeadMatrixOf(const BodyPose& pose, const BodyStance& stance);

// A body in its standing pose (frame 0 of bodyWait, upright) and in the size of its Mii, as triangle corners in the space of the body.
struct PosedBody
{
    std::vector<Graphics::MeshVertex> shirt;
    std::vector<Graphics::MeshVertex> pants;
    Formats::Vector3 headPosition;
};

PosedBody PoseStandingBody(const MiiBodyFile& file, const MiiRecord& record);

// The pants of the Mii Channel: gold for a special Mii, gray for a Mii of this console (red for a favorite), blue for others.
Formats::Rgba8Color PantsColorFor(const MiiRecord& record, std::uint32_t consoleSystemId);

// The two bodies of the Mii Channel on the graphics card, shared by every Mii: shirt and pants with their corners in the space of their
// bones, and the lit sphere that shades them.
class MiiBodyModels
{
public:
    MiiBodyModels(Graphics::GraphicsDevice& graphicsDevice, const MiiBodyFiles& bodyFiles);
    ~MiiBodyModels();

    MiiBodyModels(const MiiBodyModels&) = delete;
    MiiBodyModels& operator=(const MiiBodyModels&) = delete;

    const MiiBodyFile& FileFor(Gender gender) const;

    // Draws shirt and pants of the Mii in the pose: their color plus the square of the lit sphere at the direction of the surface.
    // The body has no light of its own, so the light of the placement is not used.
    void Draw(const MiiRecord& record, std::uint32_t consoleSystemId, const BodyPose& pose, const Graphics::MeshPlacement& placement) const;

private:
    struct GenderModel
    {
        Graphics::MeshHandle shirt;
        Graphics::MeshHandle pants;
    };

    GenderModel MakeModel(const MiiBodyFile& file);
    const GenderModel& ModelFor(Gender gender) const;
    void DrawPart(Graphics::MeshHandle part, const BodyPose& pose, const Graphics::MeshPlacement& placement, Formats::Rgba8Color color) const;

    Graphics::GraphicsDevice& device;
    const MiiBodyFiles& files;
    GenderModel male;
    GenderModel female;
    Graphics::TextureHandle environmentTexture;
};

}
