#include "Mii/MiiIcon.h"

#include "Mii/MiiHead.h"

#include <cmath>

namespace Mii
{

namespace
{

// The camera of the icons: 600 units in front of the face at the height of 34.5, looking straight at it.
constexpr Formats::Vector3 IconEye = {0.0f, 34.5f, 600.0f};
constexpr Formats::Vector3 IconTarget = {0.0f, 34.5f, 0.0f};
constexpr Formats::Vector3 IconUp = {0.0f, 1.0f, 0.0f};
constexpr float NearDistance = 500.0f;
constexpr float FarDistance = 700.0f;

// The shorter side of the icon sees 43.2 units to each side at the near distance.
constexpr float HalfViewAtNearDistance = 43.2f;

constexpr float DegreesPerRadian = 57.29578f;

constexpr std::uint32_t SmallFaceTexture = 64;
constexpr std::uint32_t MiddleFaceTexture = 128;
constexpr std::uint32_t LargeFaceTexture = 256;

// The face texture is as big as the icon needs: 64, 128 or 256 pixels.
std::uint32_t FaceTextureSizeFor(std::uint32_t width, std::uint32_t height)
{
    if (width <= SmallFaceTexture && height <= SmallFaceTexture)
        return SmallFaceTexture;

    if (width <= MiddleFaceTexture && height <= MiddleFaceTexture)
        return MiddleFaceTexture;

    return LargeFaceTexture;
}

}

Graphics::MeshPlacement MakeIconPlacement(float aspect)
{
    float halfView = HalfViewAtNearDistance;
    if (aspect < 1.0f)
        halfView = HalfViewAtNearDistance / aspect;

    float verticalAngle = 2.0f * std::atan(halfView / NearDistance) * DegreesPerRadian;
    Graphics::Matrix4 view = Graphics::MakeLookAt(IconEye, IconTarget, IconUp);

    Graphics::MeshPlacement placement;
    placement.modelView = view;
    placement.projection = Graphics::MakePerspective(verticalAngle, aspect, NearDistance, FarDistance);
    placement.light = MakeHeadLight(view);
    return placement;
}

Graphics::TextureHandle MakeMiiIcon(Graphics::GraphicsDevice& device, const MiiResources& resources, const MiiRecord& record,
    std::uint32_t width, std::uint32_t height, MiiExpression expression)
{
    MiiHead head(device, resources, record, expression, FaceTextureSizeFor(width, height));

    auto canvasWidth = static_cast<float>(width);
    auto canvasHeight = static_cast<float>(height);

    Graphics::TextureHandle icon = device.BeginOffscreen({canvasWidth, canvasHeight}, width, height);
    head.Draw(MakeIconPlacement(canvasWidth / canvasHeight));
    device.EndOffscreen();
    return icon;
}

}
