#pragma once

#include "Mii/MiiBodyFile.h"
#include "Mii/MiiExpression.h"
#include "Mii/MiiRecord.h"
#include "Mii/MiiResources.h"

#include "Graphics/GraphicsDevice.h"

#include <cstdint>

namespace Mii
{

// Draws a whole Mii standing upright, body and head, into a new texture of width x height pixels, seen from the front on a
// transparent ground. The consoleSystemId decides the color of the pants. The caller destroys the texture.
// It must not be called while an offscreen picture is being drawn.
Graphics::TextureHandle MakeStandingMiiPicture(Graphics::GraphicsDevice& device, const MiiResources& resources, const MiiBodyFiles& bodies,
    const MiiRecord& record, std::uint32_t consoleSystemId, std::uint32_t width, std::uint32_t height,
    MiiExpression expression = MiiExpression::Normal);

}
