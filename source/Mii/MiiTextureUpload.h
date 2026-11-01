#pragma once

#include "Mii/MiiResources.h"

#include "Graphics/GraphicsDevice.h"

namespace Mii
{

enum class ColorChannel
{
    Red,
    Green,
    Blue,
};

// The picture with one color channel copied into red, green and blue; the alpha stays. The eye and mouth pictures keep a
// different mask in each channel, and each mask is colored by a stage of its own.
Formats::RgbaImage SpreadChannel(const Formats::RgbaImage& image, ColorChannel channel);

// The picture with green and alpha exchanged. The make-up pictures keep their blend weight in green and the green of their color
// in alpha; the face library reads them through a swap table that does the same.
Formats::RgbaImage SwapGreenAndAlpha(const Formats::RgbaImage& image);

// Puts a picture on the graphics device, filtered bilinearly. The face texture's parts are clamped at their edges; the textures of
// the 3D parts use the wrap modes stored with them.
Graphics::TextureHandle UploadClampedTexture(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image);
Graphics::TextureHandle UploadTexture(Graphics::GraphicsDevice& device, const Formats::RgbaImage& image, const MiiTexture& storedSettings);

}
