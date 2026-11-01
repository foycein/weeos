#pragma once

#include "Mii/MiiExpression.h"
#include "Mii/MiiHeadModel.h"
#include "Mii/MiiRecord.h"
#include "Mii/MiiResources.h"

#include "Graphics/GraphicsDevice.h"

#include <cstdint>

namespace Mii
{

// A Mii's head ready to draw: its 3D parts and its textures on the graphics device, the face texture showing one expression.
class MiiHead
{
public:
    // Builds the parts and draws the face texture, so it must not be made while an offscreen picture is being drawn.
    MiiHead(Graphics::GraphicsDevice& graphicsDevice, const MiiResources& resources, const MiiRecord& miiRecord, MiiExpression expression,
        std::uint32_t faceTextureSize);
    ~MiiHead();

    MiiHead(const MiiHead&) = delete;
    MiiHead& operator=(const MiiHead&) = delete;

    // Draws the opaque parts and then the translucent ones the way the face library does (docs/research/Mii.md section 8).
    // The placement gives the matrices and the light; the head sets the culling and the depth of each part itself.
    void Draw(const Graphics::MeshPlacement& placement) const;

private:
    void DrawPart(const std::vector<Graphics::MeshVertex>& corners, const Graphics::MeshPlacement& placement,
        Graphics::FaceCulling culling, Graphics::DepthMode depth, Graphics::Material material) const;

    Graphics::GraphicsDevice& device;
    MiiRecord record;
    MiiHeadModel model;
    Graphics::TextureHandle faceTexture;
    Graphics::TextureHandle makeUpTexture;
    Graphics::TextureHandle capTexture;
    Graphics::TextureHandle noselineTexture;
    Graphics::TextureHandle glassesTexture;
};

}
