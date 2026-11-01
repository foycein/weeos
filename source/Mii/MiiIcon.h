#pragma once

#include "Mii/MiiExpression.h"
#include "Mii/MiiRecord.h"
#include "Mii/MiiResources.h"

#include "Graphics/GraphicsDevice.h"

#include <cstdint>

namespace Mii
{

// The System Menu's Mii chooser, address book and Message Board show 76 x 76 icons.
constexpr std::uint32_t MenuIconSize = 76;

// Draws the head of a Mii into a new texture of width x height pixels, the way the face library makes icons
// (docs/research/Mii.md section 9.2): the face from the front, lit from the upper right, on a transparent ground, filling the
// shorter side. A layout pane can show the texture. The caller destroys it. It must not be called while an offscreen picture is
// being drawn.
Graphics::TextureHandle MakeMiiIcon(Graphics::GraphicsDevice& device, const MiiResources& resources, const MiiRecord& record,
    std::uint32_t width, std::uint32_t height, MiiExpression expression = MiiExpression::Normal);

// The camera and the light of an icon of this shape, for drawing a head the same way into another picture.
Graphics::MeshPlacement MakeIconPlacement(float aspect);

}
