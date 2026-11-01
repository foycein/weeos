#include "Channels/ShopAccessMark.h"

#include "Formats/ResourceFile.h"

#include <cmath>
#include <numbers>
#include <string_view>

namespace Channels
{

namespace
{

constexpr std::array<std::string_view, 2> MarkPictureNames = {"AccessMark.1", "AccessMark.2"};
constexpr std::array<std::string_view, 2> ShadowPictureNames = {"AccessShadow.1", "AccessShadow.2"};

// The animation AccessMark1 turns the mark once clockwise in 599 frames, played 10 frames a step in the USA (0x802CEA34): a turn a second.
constexpr float FramesOfATurn = 599;
constexpr float FramesPerStep = 10;
constexpr float FullTurnDegrees = 360;

// The model: a square of 4 units, the shadow 0.4 units right and down of the mark, with the alpha 0x2B of its corners.
constexpr float HalfSideOfTheSquare = 2;
constexpr float ShadowOffset = 0.4f;
constexpr std::uint8_t ShadowAlpha = 0x2B;

// The camera of the mark (0x8001CA70): an orthographic box 47.6 units wide and 28 high around (0, 6.5), drawn into a viewport of 640
// by 480 lines from the top of the picture. On the canvas of 832 by 456: x * 832 / 47.6, and the 480 lines of the viewport reach 24 lines
// below the picture (AccessMarkViewportLines, Assumed: that the viewport starts at the top of the picture).
constexpr float BoxWidth = 47.6f;
constexpr float BoxHeight = 28;
constexpr float BoxTop = 6.5f + BoxHeight / 2;
constexpr float CanvasWidth = 832;
constexpr float CanvasTop = 228;
constexpr float AccessMarkViewportLines = 480;
constexpr float CanvasUnitsPerBoxUnitAcross = CanvasWidth / BoxWidth;
constexpr float CanvasUnitsPerBoxUnitUp = AccessMarkViewportLines / BoxHeight;

// Where and how large the mark stands (0x8001E380, 0x8001E510): 1.6 times at (0, 3) over the start-up picture, 1.2 times at
// (-15, 17.75) over a page.
struct MarkPlacement
{
    Formats::Vector2 middle;
    float scale;
};

constexpr MarkPlacement OverTheStartUpPicture = {{0, 3}, 1.6f};
constexpr MarkPlacement OverThePage = {{-15, 17.75f}, 1.2f};

Formats::Vector2 BoxToCanvas(Formats::Vector2 point)
{
    return {point.x * CanvasUnitsPerBoxUnitAcross, CanvasTop - (BoxTop - point.y) * CanvasUnitsPerBoxUnitUp};
}

Graphics::TextureHandle Upload(Graphics::GraphicsDevice& device, Formats::ByteView file, std::string_view name)
{
    Formats::RgbaImage image = Formats::ReadResourceTexture(file, name);
    Graphics::TextureDescription description;
    description.width = image.width;
    description.height = image.height;
    description.pixels = image.pixels;
    return device.CreateTexture(description);
}

// A square of the model around its middle, turned clockwise, in canvas units.
Graphics::Quad TurnedSquare(Formats::Vector2 middle, float scale, float turnDegrees, std::uint8_t alpha)
{
    float radians = -turnDegrees * std::numbers::pi_v<float> / 180;
    float cosine = std::cos(radians);
    float sine = std::sin(radians);
    float half = HalfSideOfTheSquare * scale;

    Graphics::Quad quad;
    const std::array<Formats::Vector2, Graphics::QuadCornerCount> corners = {{{-half, half}, {half, half}, {half, -half}, {-half, -half}}};
    const std::array<Formats::Vector2, Graphics::QuadCornerCount> pictureCorners = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    for (std::size_t corner = 0; corner < Graphics::QuadCornerCount; ++corner)
    {
        Formats::Vector2 turned = {cosine * corners[corner].x - sine * corners[corner].y, sine * corners[corner].x + cosine * corners[corner].y};
        quad.positions[corner] = BoxToCanvas({middle.x + turned.x, middle.y + turned.y});
        quad.colors[corner] = {255, 255, 255, alpha};
        quad.textureCoordinates[0][corner] = pictureCorners[corner];
    }
    return quad;
}

Graphics::Material PictureMaterial(Graphics::TextureHandle picture)
{
    Graphics::Material material;
    material.textures.push_back({picture, {}});
    return material;
}

}

ShopAccessMark::ShopAccessMark(Graphics::GraphicsDevice& graphicsDevice, Formats::ByteView accessMarkFile)
    : device(graphicsDevice)
{
    for (std::size_t frame = 0; frame < markPictures.size(); ++frame)
    {
        markPictures[frame] = Upload(device, accessMarkFile, MarkPictureNames[frame]);
        shadowPictures[frame] = Upload(device, accessMarkFile, ShadowPictureNames[frame]);
    }
}

ShopAccessMark::~ShopAccessMark()
{
    for (std::size_t frame = 0; frame < markPictures.size(); ++frame)
    {
        device.DestroyTexture(markPictures[frame]);
        device.DestroyTexture(shadowPictures[frame]);
    }
}

void ShopAccessMark::Update()
{
    frame = std::fmod(frame + FramesPerStep, FramesOfATurn);
}

float ShopAccessMark::TurnDegrees() const
{
    return frame / FramesOfATurn * FullTurnDegrees;
}

// The texture pattern shows its first pictures over the start-up picture and its second over a page.
void ShopAccessMark::Draw(AccessMarkPlace place) const
{
    bool isOverThePage = place == AccessMarkPlace::OverThePage;
    const MarkPlacement& placement = isOverThePage ? OverThePage : OverTheStartUpPicture;
    std::size_t pictureNumber = isOverThePage ? 1 : 0;

    Formats::Vector2 shadowMiddle = {placement.middle.x + ShadowOffset * placement.scale, placement.middle.y - ShadowOffset * placement.scale};
    device.DrawQuad(TurnedSquare(shadowMiddle, placement.scale, TurnDegrees(), ShadowAlpha), PictureMaterial(shadowPictures[pictureNumber]));
    device.DrawQuad(TurnedSquare(placement.middle, placement.scale, TurnDegrees(), 255), PictureMaterial(markPictures[pictureNumber]));
}

}
