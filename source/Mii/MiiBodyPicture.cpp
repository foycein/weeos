#include "Mii/MiiBodyPicture.h"

#include "Mii/MiiBody.h"
#include "Mii/MiiHead.h"

#include "Graphics/Matrix4.h"

namespace Mii
{

namespace
{

// Assumed (docs/audit/OpenPoints/Mii.md MII-20): the camera of the Mii Plaza was not traced. This one looks at the Mii from the
// front and shows the tallest one (about 220 units with its hair) whole.
struct StandingMiiCamera
{
    static constexpr Formats::Vector3 Eye = {0.0f, 110.0f, 700.0f};
    static constexpr Formats::Vector3 Target = {0.0f, 110.0f, 0.0f};
    static constexpr Formats::Vector3 Up = {0.0f, 1.0f, 0.0f};
    static constexpr float VerticalAngleDegrees = 19.5f;
    static constexpr float NearDistance = 100.0f;
    static constexpr float FarDistance = 2000.0f;
};

constexpr std::uint32_t FaceTextureSize = 128;

}

Graphics::TextureHandle MakeStandingMiiPicture(Graphics::GraphicsDevice& device, const MiiResources& resources, const MiiBodyFiles& bodies,
    const MiiRecord& record, std::uint32_t consoleSystemId, std::uint32_t width, std::uint32_t height, MiiExpression expression)
{
    MiiHead head(device, resources, record, expression, FaceTextureSize);
    MiiBodyModels bodyModels(device, bodies);
    BodyPose pose = PoseBody(bodyModels.FileFor(record.gender), record, BodyStance());

    auto canvasWidth = static_cast<float>(width);
    auto canvasHeight = static_cast<float>(height);
    Graphics::Matrix4 view = Graphics::MakeLookAt(StandingMiiCamera::Eye, StandingMiiCamera::Target, StandingMiiCamera::Up);

    Graphics::MeshPlacement bodyPlacement;
    bodyPlacement.modelView = view;
    bodyPlacement.projection = Graphics::MakePerspective(StandingMiiCamera::VerticalAngleDegrees, canvasWidth / canvasHeight,
        StandingMiiCamera::NearDistance, StandingMiiCamera::FarDistance);

    // The head stands on the head bone, upright and in its own size, lit as in the icons.
    Graphics::MeshPlacement headPlacement = bodyPlacement;
    headPlacement.modelView = view * Graphics::MakeTranslation(pose.headPosition);
    headPlacement.light = MakeHeadLight(headPlacement.modelView);

    Graphics::TextureHandle picture = device.BeginOffscreen({canvasWidth, canvasHeight}, width, height);
    bodyModels.Draw(record, consoleSystemId, pose, bodyPlacement);
    head.Draw(headPlacement);
    device.EndOffscreen();
    return picture;
}

}
