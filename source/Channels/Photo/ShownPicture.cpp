#include "Channels/Photo/ShownPicture.h"

#include <algorithm>

namespace Channels
{

namespace
{

constexpr float FullOpacity = 255;
constexpr Formats::Byte White = 255;

}

ShownPicture::ShownPicture(Graphics::GraphicsDevice& graphicsDevice, const Formats::RgbaImage& picture)
    : device(graphicsDevice)
    , width(static_cast<float>(picture.width))
    , height(static_cast<float>(picture.height))
{
    Graphics::TextureDescription description;
    description.width = picture.width;
    description.height = picture.height;
    description.pixels = picture.pixels;
    texture = device.CreateTexture(description);
}

ShownPicture::~ShownPicture()
{
    device.DestroyTexture(texture);
}

void ShownPicture::DrawInside(const PictureBox& box, float opacity, float zoom) const
{
    float scale = std::min(box.width / width, box.height / height) * zoom;
    float halfWidth = width * scale / 2;
    float halfHeight = height * scale / 2;
    float left = box.centerX - halfWidth;
    float right = box.centerX + halfWidth;
    float top = box.centerY + halfHeight;
    float bottom = box.centerY - halfHeight;

    Formats::Byte alpha = static_cast<Formats::Byte>(std::clamp(opacity, 0.0f, 1.0f) * FullOpacity);
    Formats::Rgba8Color color = {White, White, White, alpha};

    Graphics::Quad quad{};
    quad.positions = {Formats::Vector2{left, top}, {right, top}, {right, bottom}, {left, bottom}};
    quad.colors = {color, color, color, color};
    quad.textureCoordinates[0] = {Formats::Vector2{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    Graphics::Material material;
    material.textures.push_back({texture, Graphics::TextureMatrix()});
    device.DrawQuad(quad, material);
}

void ShownPicture::DrawPart(const PictureBox& box, Formats::Vector2 partTopLeft, Formats::Vector2 partBottomRight) const
{
    float left = box.centerX - box.width / 2;
    float right = box.centerX + box.width / 2;
    float top = box.centerY + box.height / 2;
    float bottom = box.centerY - box.height / 2;
    Formats::Rgba8Color white = {White, White, White, White};

    Graphics::Quad quad{};
    quad.positions = {Formats::Vector2{left, top}, {right, top}, {right, bottom}, {left, bottom}};
    quad.colors = {white, white, white, white};
    quad.textureCoordinates[0] = {partTopLeft, Formats::Vector2{partBottomRight.x, partTopLeft.y}, partBottomRight,
        Formats::Vector2{partTopLeft.x, partBottomRight.y}};

    Graphics::Material material;
    material.textures.push_back({texture, Graphics::TextureMatrix()});
    device.DrawQuad(quad, material);
}

}
