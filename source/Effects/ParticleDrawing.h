#pragma once

#include "Effects/Effect.h"

#include "Graphics/GraphicsDevice.h"
#include "Graphics/Matrix4.h"

#include <optional>

namespace Effects
{

// How a program sees its effects.
struct EffectCamera
{
    // From the world into view space: x to the right, y up, the viewer looking along -z.
    Graphics::Matrix4 view;

    // Effects among 3D models: the projection of view space into the picture. The particles are then drawn with the depth buffer of
    // the picture, behind and in front of the models.
    std::optional<Graphics::Matrix4> projection;

    // Flat effects (no projection): view space is the box of the program's orthographic projection, laid onto the canvas as
    // canvas = view * boxToCanvasScale + boxToCanvasOffset.
    Formats::Vector2 boxToCanvasScale = {1, 1};
    Formats::Vector2 boxToCanvasOffset = {0, 0};
};

void DrawEffect(const Effect& effect, const EffectCamera& camera);

// The blend of the graphics chip that a draw setting asks for, when WeeOS has it.
std::optional<Graphics::BlendMode> BlendModeOf(const Formats::EffectDrawSetting& drawSetting);

// The texture matrix of one of a particle's textures: its translation, then its rotation and scale around the middle, then the
// reverse and the mirror of its wrap.
Graphics::TextureMatrix ParticleTextureMatrix(const Formats::TextureTransform& transform, std::uint8_t wrapBits, std::uint8_t reverseBits);

}
