#pragma once

#include "Graphics/GraphicsDevice.h"

#include "Formats/RgbaImage.h"

namespace Channels
{

// A place of the picture in canvas units: the middle is (0, 0) and y grows upward.
struct PictureBox
{
    float centerX = 0;
    float centerY = 0;
    float width = 0;
    float height = 0;
};

// A photo or a frame of a movie as a texture of the graphics device, drawn as large as it fits into a box without changing its shape.
class ShownPicture
{
public:
    ShownPicture(Graphics::GraphicsDevice& graphicsDevice, const Formats::RgbaImage& picture);
    ShownPicture(const ShownPicture&) = delete;
    ShownPicture& operator=(const ShownPicture&) = delete;
    ~ShownPicture();

    // Opacity from 0 (not seen) to 1. The picture is magnified by zoom around the middle of the box.
    void DrawInside(const PictureBox& box, float opacity = 1, float zoom = 1) const;

private:
    Graphics::GraphicsDevice& device;
    Graphics::TextureHandle texture;
    float width;
    float height;
};

}
