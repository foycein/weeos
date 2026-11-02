#pragma once

#include "Mii/MiiRecord.h"
#include "Mii/MiiResources.h"

#include "Graphics/GraphicsDevice.h"

#include <cstdint>

namespace Mii
{

// Draws the face texture of a Mii into a new texture of size x size pixels: mustache, mouth, eyebrows, eyes and mole on a
// transparent ground. The record is the one of the expression to show (ApplyExpression).
// The caller destroys the texture. It must not be called while an offscreen picture is being drawn.
Graphics::TextureHandle DrawMiiFaceTexture(Graphics::GraphicsDevice& device, const MiiResources& resources, const MiiRecord& record,
    std::uint32_t size);

}
